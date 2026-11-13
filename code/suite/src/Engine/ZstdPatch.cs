using System;
using System.ComponentModel;
using System.IO;
using System.Runtime.InteropServices;

namespace GtavxrSuite {
    internal static class ZstdPatch {
        [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
        static extern IntPtr LoadLibraryExW(string path, IntPtr file, uint flags);
        [DllImport("kernel32.dll", CharSet = CharSet.Ansi, ExactSpelling = true, SetLastError = true)]
        static extern IntPtr GetProcAddress(IntPtr library, string name);
        [DllImport("kernel32.dll")]
        static extern bool FreeLibrary(IntPtr library);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate IntPtr CreateContext();
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate UIntPtr FreeContext(IntPtr context);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate ulong FrameContentSize(IntPtr input, UIntPtr length);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)] delegate UIntPtr FrameCompressedSize(IntPtr input, UIntPtr length);
        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        delegate UIntPtr Decompress(IntPtr context, IntPtr target, UIntPtr capacity, IntPtr input, UIntPtr length,
                                   IntPtr dictionary, UIntPtr dictLength);

        static T Export<T>(IntPtr library, string name) where T : class {
            IntPtr symbol = GetProcAddress(library, name);
            if (symbol == IntPtr.Zero) throw new InvalidDataException("DELTA_CODEC_EXPORT:" + name);
            return Marshal.GetDelegateForFunctionPointer(symbol, typeof(T)) as T;
        }

        static UIntPtr Size(byte[] data) { return new UIntPtr((uint)data.Length); }

        internal static byte[] Apply(string path, byte[] basis, byte[] patch, int targetBytes) {
            if (patch.Length < 9 || patch[0] != 0x28 || patch[1] != 0xb5 || patch[2] != 0x2f || patch[3] != 0xfd ||
                (patch[4] & 4) == 0) throw new InvalidDataException("DELTA_FRAME_HEADER");
            const uint SearchDllDirectoryAndSystem32 = 0x100 | 0x800;
            IntPtr library = LoadLibraryExW(path, IntPtr.Zero, SearchDllDirectoryAndSystem32);
            if (library == IntPtr.Zero) throw new Win32Exception(Marshal.GetLastWin32Error(), "DELTA_CODEC_LOAD");
            try {
                var contentSize = Export<FrameContentSize>(library, "ZSTD_getFrameContentSize");
                var compressedSize = Export<FrameCompressedSize>(library, "ZSTD_findFrameCompressedSize");
                var create = Export<CreateContext>(library, "ZSTD_createDCtx");
                var release = Export<FreeContext>(library, "ZSTD_freeDCtx");
                var decode = Export<Decompress>(library, "ZSTD_decompress_usingDict");
                using (var input = new PinnedBytes(patch)) {
                    if (contentSize(input.Address, Size(patch)) != (ulong)targetBytes ||
                        compressedSize(input.Address, Size(patch)) != Size(patch))
                        throw new InvalidDataException("DELTA_FRAME_SIZE");
                    var result = new byte[targetBytes];
                    IntPtr context = create();
                    if (context == IntPtr.Zero) throw new InvalidDataException("DELTA_CODEC_CONTEXT");
                    try {
                        using (var output = new PinnedBytes(result))
                        using (var dictionary = new PinnedBytes(basis)) {
                            if (decode(context, output.Address, Size(result), input.Address, Size(patch),
                                       dictionary.Address, Size(basis)) != Size(result))
                                throw new InvalidDataException("DELTA_DECODE_FAILED");
                        }
                    } finally { release(context); }
                    return result;
                }
            } finally { FreeLibrary(library); }
        }

        sealed class PinnedBytes : IDisposable {
            GCHandle handle;
            public PinnedBytes(byte[] bytes) { handle = GCHandle.Alloc(bytes, GCHandleType.Pinned); }
            public IntPtr Address { get { return handle.AddrOfPinnedObject(); } }
            public void Dispose() { handle.Free(); }
        }
    }
}

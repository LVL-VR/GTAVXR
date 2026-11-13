using System;
using System.Collections.Generic;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;

namespace GtavxrSuite {
    public class EmbeddedIdentity {
        public long bytes { get; set; }
        public string sha256 { get; set; }
    }

    public sealed class EmbeddedEntry : EmbeddedIdentity {
        public string path { get; set; }
        public string kind { get; set; }
        public string retail_archive { get; set; }
        public string retail_entry { get; set; }
        public EmbeddedIdentity retail_base { get; set; }
        public EmbeddedIdentity shader_patch { get; set; }
    }

    public sealed class EmbeddedManifest {
        public string schema { get; set; }
        public string id { get; set; }
        public EmbeddedIdentity @base { get; set; }
        public EmbeddedIdentity target { get; set; }
        public List<EmbeddedEntry> entries { get; set; }
    }

    public static class EmbeddedRelease {
        const string Resource = "GtavxrSuite.Distribution.Bundle";
        const int MiB = 1024 * 1024;
        const string Exe = "GTAVXR_SourceBaseline.exe";
        const string ReleaseName = "embedded-gtavxr-1-0-0";
        const string BaseHash = "677E4E355CFBDB13273B1D992407E3C261B3A108DC4DD5C8A0C4C1DA651802E5";
        const string TargetHash = "814A9DC6BE3CF49C888B5C76D77842149532691607D9F2DC54F5795B4D623CCE";
        const string CodecHash = "8F07E1112ED283E5CD2798833E9A3C32D8961381BC36DA04AF57A1B0CA9BD40B";
        const string Table = "scripthost/ScriptHookVDotNet.gtavxr-memory.txt";
        internal const string VersionId = "gtavxr-1-0-0";

        static readonly HashSet<string> Runtime = new HashSet<string>(StringComparer.Ordinal) {
            "GTAVXRHost.dll", "gtavxr_native_openxr.dll", "gtavxr_trainer.dll", "openxr_loader.dll",
            "nvngx.dll_gtavxr.dll", "nvngx.dll_gtavxr_sr.dll", "tools/native_fault_monitor.exe",
            "scripthost/ScriptHookVDotNet.asi", "scripthost/ScriptHookVDotNet2.dll", "scripthost/ScriptHookVDotNet3.dll",
            "scripthost/ScriptHookVDotNet.ini", "scripthost/gtavxr-host-features.txt", Table,
            "scripthost/ScriptHookVDotNet-LICENSE.txt", "scripthost/ScriptHookVDotNet-THIRD-PARTY-NOTICES.md"
        };

        public static bool Available {
            get { return Assembly.GetExecutingAssembly().GetManifestResourceInfo(Resource) != null; }
        }

        static byte[] Descriptor(CancellationToken cancel) {
            using (var resource=Assembly.GetExecutingAssembly().GetManifestResourceStream(Resource)) {
                Require(resource!=null && resource.Length<=320L*MiB,"BUNDLE_UNAVAILABLE");
                using (var archive=new ZipArchive(resource,ZipArchiveMode.Read)) {
                    var manifest=archive.GetEntry("manifest.json");
                    Require(manifest!=null && manifest.Length<=MiB,"BUNDLE_MANIFEST");
                    using (var stream=manifest.Open()) return ReadExact(stream,(int)manifest.Length,cancel);
                }
            }
        }

        internal static string DescriptorIdentity(CancellationToken cancel) { return Hash(Descriptor(cancel)); }

        internal static void VerifyRetailInputs(string game,CancellationToken cancel) {
            var manifest=new JavaScriptSerializer { MaxJsonLength=MiB }.Deserialize<EmbeddedManifest>(
                new UTF8Encoding(false,true).GetString(Descriptor(cancel)));
            try { SafeFs.AssertFile(SafeFs.Under(game,"GTA5.exe"),manifest.@base.sha256,manifest.@base.bytes); }
            catch(InvalidDataException error) { throw new InvalidDataException("DELTA_BASE_MISMATCH",error); }
            var retail=new Content.GameFiles(game);
            foreach(var entry in manifest.entries.Where(row=>row.retail_archive!=null)) {
                cancel.ThrowIfCancellationRequested();
                ReadRetailMember(retail,entry);
            }
        }

        static byte[] ReadRetailMember(Content.GameFiles retail,EmbeddedEntry entry) {
            var expected=entry.retail_base??entry;
            var reason=(entry.retail_base!=null?"RETAIL_SHADER_BASE_MISMATCH:":"RETAIL_RESOURCE_MISMATCH:")+entry.path;
            try {
                var archive=retail.Archive(entry.retail_archive);
                var member=archive==null?null:archive.Find(entry.retail_entry);
                Require(member!=null,reason);
                var bytes=archive.Extract(member,checked((int)expected.bytes));
                Require(bytes.Length==expected.bytes&&Hash(bytes)==expected.sha256,reason);
                return bytes;
            } catch(InvalidDataException error) {
                if(error.Message==reason) throw;
                throw new InvalidDataException(reason,error);
            } catch(EndOfStreamException error) { throw new InvalidDataException(reason,error); }
        }

        static string Hash(byte[] bytes) {
            using (var sha = SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(bytes)).Replace("-", "");
        }

        static void Require(bool condition, string error) {
            if (!condition) throw new InvalidDataException(error);
        }

        static void CheckIdentity(EmbeddedIdentity row, int max) {
            Require(row != null && row.bytes > 0 && row.bytes <= max && SafeFs.IsHash(row.sha256), "BUNDLE_IDENTITY");
        }

        static byte[] ReadExact(Stream stream, int size, CancellationToken cancel) {
            var bytes = new byte[size];
            int read = 0;
            while (read < size) {
                cancel.ThrowIfCancellationRequested();
                int n = stream.Read(bytes, read, Math.Min(MiB, size - read));
                Require(n > 0, "BUNDLE_TRUNCATED");
                read += n;
            }
            Require(stream.ReadByte() == -1, "BUNDLE_LENGTH");
            return bytes;
        }

        static bool Allowed(EmbeddedEntry entry) {
            if (entry.kind == "patch") return entry.path == "target.zst";
            if (entry.kind == "codec") return entry.path == "libzstd.dll";
            if (entry.kind == "metadata") return entry.path == "suite-launch.json" || entry.path == "trial-profile/vr-settings.ini";
            if (entry.kind == "runtime") return Runtime.Contains(entry.path);
            if (entry.kind != "payload") return false;
            if (entry.path == "gtavxr_mvga147/platform/textures/graphics.ytd" ||
                entry.path == "gtavxr_mvga147/platform/textures/graphics_pc.ytd" ||
                entry.path == "gtavxr_mvga147/platform/textures/fxweather.ytd") return true;
            string ext = Path.GetExtension(entry.path).ToLowerInvariant();
            return (entry.path.StartsWith("common/", StringComparison.Ordinal) ||
                    entry.path.StartsWith("gtavxr_mvga147/common/", StringComparison.Ordinal)) &&
                   new[] { ".fx", ".fxc", ".sps", ".list", ".xml", ".dat", ".ytd" }.Contains(ext);
        }

        static string ShaderPatchName(string path) { return "shader-deltas/" + path + ".zst"; }

        static bool ShaderDeltaAllowed(EmbeddedEntry entry) {
            return entry.kind == "payload" &&
                   (((entry.path.StartsWith("common/shaders/win32_40/", StringComparison.Ordinal) ||
                      entry.path.StartsWith("common/shaders/win32_50/", StringComparison.Ordinal)) &&
                     Path.GetExtension(entry.path) == ".fxc") ||
                    (entry.path == "common/shaders/fx_max/shaderbuild.fx" && entry.retail_archive == null));
        }

        static byte[] ReadMember(ZipArchiveEntry member, EmbeddedIdentity identity, CancellationToken cancel) {
            byte[] bytes;
            using (var stream = member.Open()) bytes = ReadExact(stream, (int)identity.bytes, cancel);
            Require(Hash(bytes) == identity.sha256, "BUNDLE_MEMBER_HASH:" + member.FullName);
            return bytes;
        }

        public static object Reconstruct(string game, string destination, CancellationToken cancel) {
            using (var resource = Assembly.GetExecutingAssembly().GetManifestResourceStream(Resource)) {
                Require(resource != null, "BUNDLE_UNAVAILABLE");
                return Stage(resource, game, destination, cancel, false);
            }
        }

        internal static object Stage(Stream resource, string game, string destination, CancellationToken cancel, bool fixture) {
            game = SafeFs.Root(game);
            destination = SafeFs.Root(destination);
            Require(!Directory.Exists(destination) && !File.Exists(destination) &&
                    !destination.StartsWith(game + Path.DirectorySeparatorChar, StringComparison.OrdinalIgnoreCase), "BUNDLE_STAGING_PATH");
            Require(resource.CanSeek && resource.Length <= 320L * MiB, "BUNDLE_SIZE");
            using (var archive = new ZipArchive(resource, ZipArchiveMode.Read, true)) {
                Require(archive.Entries.Count > 4 && archive.Entries.Count <= 1200, "BUNDLE_ENTRY_COUNT");
                var zipEntries = new Dictionary<string, ZipArchiveEntry>(StringComparer.OrdinalIgnoreCase);
                foreach (var entry in archive.Entries) {
                    Require(SafeFs.Relative(entry.FullName) == entry.FullName && !zipEntries.ContainsKey(entry.FullName), "BUNDLE_PATH");
                    zipEntries.Add(entry.FullName, entry);
                }
                ZipArchiveEntry json;
                Require(zipEntries.TryGetValue("manifest.json", out json) && json.Length <= MiB, "BUNDLE_MANIFEST");
                string text;
                using (var stream = json.Open()) text = new UTF8Encoding(false, true).GetString(ReadExact(stream, (int)json.Length, cancel));
                StrictJson.Validate(text);
                var manifest = new JavaScriptSerializer { MaxJsonLength = MiB }.Deserialize<EmbeddedManifest>(text);
                Require(manifest != null && (manifest.schema == "gtavxr-embedded-release-v1" ||
                                             manifest.schema == "gtavxr-embedded-release-v2") &&
                        manifest.entries != null && manifest.entries.Count <= 1199 &&
                        !manifest.entries.Any(entry=>entry==null), "BUNDLE_MANIFEST");
                CheckIdentity(manifest.@base, 64 * MiB);
                CheckIdentity(manifest.target, 64 * MiB);
                if (!fixture) Require(manifest.id == VersionId && manifest.@base.sha256 == BaseHash &&
                                      manifest.target.sha256 == TargetHash, "BUNDLE_RELEASE_IDENTITY");

                var names = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { "manifest.json" };
                long total = 0;
                int memberCount = 1;
                foreach (var entry in manifest.entries) {
                    CheckIdentity(entry, 64 * MiB);
                    Require(SafeFs.Relative(entry.path) == entry.path && names.Add(entry.path) && Allowed(entry), "BUNDLE_ENTRY:" + entry.path);
                    if (entry.retail_archive == null) {
                        Require(entry.retail_entry == null && entry.retail_base == null, "BUNDLE_RETAIL_PROVIDER");
                    } else {
                        Require(entry.kind == "payload" && entry.path.StartsWith("common/shaders/", StringComparison.Ordinal) &&
                                !zipEntries.ContainsKey(entry.path) && SafeFs.Relative(entry.retail_entry) == entry.retail_entry &&
                                (entry.retail_archive == "common.rpf" || entry.retail_archive == "update/update2.rpf"), "BUNDLE_RETAIL_PROVIDER");
                    }
                    ZipArchiveEntry member;
                    if (entry.shader_patch != null) {
                        Require(manifest.schema == "gtavxr-embedded-release-v2" && ShaderDeltaAllowed(entry) &&
                                entry.bytes <= 8 * MiB && !zipEntries.ContainsKey(entry.path), "BUNDLE_SHADER_PATCH:" + entry.path);
                        if (entry.retail_archive != null) CheckIdentity(entry.retail_base, 8 * MiB);
                        CheckIdentity(entry.shader_patch, 8 * MiB);
                        string patchName = ShaderPatchName(entry.path);
                        Require(names.Add(patchName) && zipEntries.TryGetValue(patchName, out member) &&
                                member.Length == entry.shader_patch.bytes, "BUNDLE_SHADER_PATCH:" + entry.path);
                        memberCount++;
                    } else {
                        Require(entry.retail_base == null, "BUNDLE_RETAIL_PROVIDER");
                        if (entry.retail_archive == null) {
                            Require(zipEntries.TryGetValue(entry.path, out member) &&
                                    member.Length == entry.bytes, "BUNDLE_ENTRY:" + entry.path);
                            memberCount++;
                        }
                    }
                    total = checked(total + entry.bytes);
                }
                Require(memberCount == zipEntries.Count, "BUNDLE_MANIFEST");
                Require(total <= 320L * MiB, "BUNDLE_EXPANSION");
                if (!fixture) Require(Runtime.All(names.Contains) && names.Contains("trial-profile/vr-settings.ini") &&
                                      manifest.entries.Count(x => x.kind == "payload") == 1102, "BUNDLE_REQUIRED_FILES");
                var patch = manifest.entries.SingleOrDefault(x => x.kind == "patch");
                var codec = manifest.entries.SingleOrDefault(x => x.kind == "codec");
                Require(patch != null && codec != null && codec.bytes <= 4 * MiB && names.Contains("suite-launch.json"), "BUNDLE_REQUIRED_FILES");
                Require(codec.sha256 == CodecHash, "DELTA_CODEC_IDENTITY");

                var source = SafeFs.Under(game, "GTA5.exe");
                Require(File.Exists(source) && new FileInfo(source).Length == manifest.@base.bytes, "DELTA_BASE_MISMATCH");
                byte[] baseBytes;
                using (var stream = File.OpenRead(source)) baseBytes = ReadExact(stream, (int)manifest.@base.bytes, cancel);
                Require(Hash(baseBytes) == manifest.@base.sha256, "DELTA_BASE_MISMATCH");
                SafeFs.Budget(Path.GetDirectoryName(destination), checked((total + manifest.target.bytes) * 2));
                int local = manifest.entries.Count(entry => entry.retail_archive != null && entry.shader_patch == null);
                int shaderDeltas = manifest.entries.Count(entry => entry.shader_patch != null);
                var retail = manifest.entries.Any(entry => entry.retail_archive != null) ? new Content.GameFiles(game) : null;
                Directory.CreateDirectory(destination);
                string codecPath = SafeFs.Under(destination, codec.path);
                WriteNew(codecPath, ReadMember(zipEntries[codec.path], codec, cancel));
                foreach (var entry in manifest.entries) {
                    cancel.ThrowIfCancellationRequested();
                    if (entry == codec) continue;
                    var path = SafeFs.Under(destination, entry.path);
                    byte[] bytes;
                    if (entry.shader_patch != null) {
                        byte[] basis = Array.Empty<byte>();
                        if (entry.retail_base != null) basis=ReadRetailMember(retail,entry);
                        var frame = ReadMember(zipEntries[ShaderPatchName(entry.path)], entry.shader_patch, cancel);
                        bytes = ZstdPatch.Apply(codecPath, basis, frame, (int)entry.bytes);
                        Require(Hash(bytes) == entry.sha256, "SHADER_TARGET_MISMATCH:" + entry.path);
                    } else if (entry.retail_archive != null) {
                        bytes = ReadRetailMember(retail,entry);
                    } else {
                        bytes = ReadMember(zipEntries[entry.path], entry, cancel);
                    }
                    Directory.CreateDirectory(Path.GetDirectoryName(path));
                    WriteNew(path, bytes);
                }
                cancel.ThrowIfCancellationRequested();
                var target = ZstdPatch.Apply(codecPath, baseBytes,
                                             File.ReadAllBytes(SafeFs.Under(destination, patch.path)), (int)manifest.target.bytes);
                Require(Hash(target) == manifest.target.sha256, "DELTA_TARGET_MISMATCH");
                WriteNew(SafeFs.Under(destination, Exe), target);
                var contract = Json.Read<SuiteLaunchFile>(SafeFs.Under(destination, "suite-launch.json"));
                Require(contract.id == manifest.id && contract.schema == "gtavxr-suite2-launch-v1" &&
                        contract.session_mode == "suite" && contract.settings_mode == "suite" &&
                        contract.args != null && contract.args.Contains("{LOGFILE}"), "BUNDLE_LAUNCH_CONTRACT");
                var files = manifest.entries.Where(e => e.kind == "runtime").Select(e => Reference(destination, e.path, e)).ToList();
                var payload = manifest.entries.Where(e => e.kind == "payload").Select(e => Reference(destination, e.path, e)).ToList();
                payload.Insert(0, Reference(destination, Exe, manifest.target));
                Json.New(SafeFs.Under(destination, "release.json"), new {
                    name = ReleaseName, retail_sha256 = manifest.@base.sha256,
                    embedded_identity = Hash(Encoding.UTF8.GetBytes(text)),
                    bridge = SafeFs.Under(destination, "gtavxr_native_openxr.dll"), files = files, payloads = payload,
                    implicit_layers = new[] { new { disable = "DISABLE_XR_APILAYER_reshade_1" } },
                    fault_monitor = files.SingleOrDefault(x => x.relative == "tools/native_fault_monitor.exe")
                });
                return new { status = "PASS_EMBEDDED_RECONSTRUCTION", id = manifest.id, sha256 = manifest.target.sha256,
                             bytes = manifest.target.bytes, payloads = payload.Count, retail_assets = local, shader_deltas = shaderDeltas,
                             runtime = files.Count, gta_launched = false };
            }
        }

        internal static string Name { get { return ReleaseName; } }
        internal static string RetailIdentity { get { return BaseHash; } }

        sealed class StagedFile {
            public string relative { get; set; }
            public string source { get; set; }
            public string sha256 { get; set; }
            public long bytes { get; set; }
        }

        static StagedFile Reference(string root, string relative, EmbeddedIdentity identity) {
            return new StagedFile { relative = relative, source = SafeFs.Under(root, relative),
                                    sha256 = identity.sha256, bytes = identity.bytes };
        }

        static void WriteNew(string path, byte[] bytes) {
            using (var output = new FileStream(path, FileMode.CreateNew, FileAccess.Write, FileShare.None)) {
                output.Write(bytes, 0, bytes.Length);
                output.Flush(true);
            }
        }
    }
}

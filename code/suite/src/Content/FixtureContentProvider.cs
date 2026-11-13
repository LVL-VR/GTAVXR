using System;
using System.Collections.Generic;
using System.IO;

namespace GtavxrSuite.Content {
    // The fixture build admits synthetic archives only. Real content needs a separate provider.
    static class FixtureContentProvider {
        public static NotSupportedException Unavailable() {
            return new NotSupportedException("The fixture build has no retail content provider.");
        }
    }

    public sealed class GtaKeys {
        public static GtaKeys Load(string gameDir) { throw FixtureContentProvider.Unavailable(); }
        public static void ReadFull(Stream stream, byte[] buffer) { throw FixtureContentProvider.Unavailable(); }
    }

    public sealed class RpfEntry {
        public string Name { get; private set; }
        public string Path { get; private set; }
        public bool IsResource { get; private set; }
        public long Offset { get; private set; }
        public uint Size { get; private set; }
        public uint Uncompressed { get; private set; }
    }

    public sealed class RpfArchive {
        public static RpfArchive Open(string path, GtaKeys keys) { throw FixtureContentProvider.Unavailable(); }
        public RpfEntry Find(string path) { throw FixtureContentProvider.Unavailable(); }
        public IEnumerable<RpfEntry> Files() { throw FixtureContentProvider.Unavailable(); }
        public RpfArchive OpenNested(RpfEntry entry) { throw FixtureContentProvider.Unavailable(); }
        public byte[] Extract(RpfEntry entry) { throw FixtureContentProvider.Unavailable(); }
        internal byte[] Extract(RpfEntry entry, int bytes) { throw FixtureContentProvider.Unavailable(); }
    }

    public sealed class PsoNames {
        public void AddMetaNames() { throw FixtureContentProvider.Unavailable(); }
        public void AddFile(string path) { throw FixtureContentProvider.Unavailable(); }
        public void AddText(byte[] data) { throw FixtureContentProvider.Unavailable(); }
        public void AddXml(byte[] data) { throw FixtureContentProvider.Unavailable(); }
    }

    public sealed class Pso {
        public Pso(byte[] data, PsoNames names) { throw FixtureContentProvider.Unavailable(); }
        public static bool IsPso(byte[] data) { throw FixtureContentProvider.Unavailable(); }
        public Node Tree() { throw FixtureContentProvider.Unavailable(); }
    }

    public static class ResourceCompare {
        public static string Scenario(byte[] original, byte[] mod, Dictionary<string, object> report) {
            throw FixtureContentProvider.Unavailable();
        }
        public static string Ymap(byte[] original, byte[] mod, Dictionary<string, object> report) {
            throw FixtureContentProvider.Unavailable();
        }
        public static string Ynd(byte[] original, byte[] mod, Dictionary<string, object> report) {
            throw FixtureContentProvider.Unavailable();
        }
    }
}

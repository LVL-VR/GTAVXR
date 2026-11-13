using System;
using System.Collections.Generic;
using System.IO;
using System.Threading;

namespace GtavxrSuite {
    static class BundleFixtures {
        public static object Run(string fixtures, string fresh) {
            fixtures=SafeFs.Root(fixtures);
            fresh=SafeFs.Root(fresh);
            if (Directory.Exists(fresh)) throw new InvalidDataException("Fresh bundle fixture destination required");
            Directory.CreateDirectory(fresh);
            string game=SafeFs.Under(fixtures,"synthetic-game");
            var expected=Json.Read<EmbeddedIdentity>(SafeFs.Under(fixtures,"expected.json"));
            var tests=new Dictionary<string,string> {
                {"valid",null},{"bad-checksum","DELTA_DECODE_FAILED"},{"trailing-frame","DELTA_FRAME_SIZE"},
                {"wrong-target","DELTA_TARGET_MISMATCH"},{"wrong-size","DELTA_FRAME_SIZE"},
                {"wrong-base","DELTA_BASE_MISMATCH"},{"member-drift","BUNDLE_MEMBER_HASH:common/example.dat"},
                {"duplicate","BUNDLE_PATH"},{"traversal","Unsafe path segment"},{"extra-member","BUNDLE_MANIFEST"}
            };
            var results=new List<object>();
            foreach (var test in tests) {
                string stage=SafeFs.Under(fresh,test.Key);
                string error=null;
                try {
                    using (var bundle=File.OpenRead(SafeFs.Under(fixtures,test.Key+".zip")))
                        EmbeddedRelease.Stage(bundle,game,stage,CancellationToken.None,true);
                } catch (InvalidDataException failure) { error=failure.Message; }
                if (error!=test.Value) throw new InvalidDataException("BUNDLE_FIXTURE:"+test.Key+":"+error);
                if (test.Value==null) SafeFs.AssertFile(SafeFs.Under(stage,"GTAVXR_SourceBaseline.exe"),expected.sha256,expected.bytes);
                else if (File.Exists(SafeFs.Under(stage,"release.json"))) throw new InvalidDataException("INVALID_FIXTURE_COMMITTED");
                results.Add(new { test=test.Key, error=error });
            }
            return new { status="PASS_SYNTHETIC_BUNDLE_FIXTURES", cases=results, gta_launched=false };
        }
    }
}

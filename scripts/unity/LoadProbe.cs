// Asks whether a stock .NET runtime can bind a Unity Mono game's assemblies, without running any game method.
// It resolves references from the game's Managed folder, except the base class library, which comes from the
// host runtime. It loads every type and JIT-prepares every non-generic method, then reports what failed to bind.
// Run it with no network, for example under `unshare -rn`.
using System; using System.Collections.Generic; using System.IO; using System.Linq; using System.Reflection;
using System.Runtime.CompilerServices; using System.Runtime.Loader;

class GameContext : AssemblyLoadContext {
    readonly string dir;
    public GameContext(string d) { dir = d; }
    static bool IsBcl(string n) => n == "mscorlib" || n == "netstandard" || n == "System" || n.StartsWith("System.") || n.StartsWith("Microsoft.");
    protected override Assembly Load(AssemblyName name) {
        if (IsBcl(name.Name)) return null; // Host runtime's copy.
        var p = Path.Combine(dir, name.Name + ".dll");
        return File.Exists(p) ? LoadFromAssemblyPath(p) : null;
    }
}

static class Program {
    static void Main(string[] args) {
        var dir = Path.GetFullPath(args[0]);
        var ctx = new GameContext(dir);
        int types = 0, typeFail = 0, methods = 0, prepared = 0, methodFail = 0, generic = 0;
        var why = new Dictionary<string, int>();
        void Note(Exception e) {
            var m = e.Message.Split('\n')[0]; m = m.Length > 140 ? m[..140] : m;
            why[m] = why.GetValueOrDefault(m) + 1;
        }
        var skip = new[] { "UnityEngine.dll", "mscorlib.dll", "netstandard.dll" };
        var names = Directory.GetFiles(dir, "*.dll").Select(Path.GetFileName)
            .Where(n => !n.StartsWith("System") && !n.StartsWith("Mono.") && !n.StartsWith("Microsoft.") && !skip.Contains(n)
                     && !(n.StartsWith("UnityEngine.") && n.EndsWith("Module.dll")));
        foreach (var n in names) {
            Assembly a;
            try { a = ctx.LoadFromAssemblyPath(Path.Combine(dir, n)); } catch (Exception e) { Note(e); continue; }
            Type[] ts;
            try { ts = a.GetTypes(); }
            catch (ReflectionTypeLoadException e) {
                ts = e.Types.Where(t => t != null).ToArray();
                typeFail += e.LoaderExceptions.Length;
                foreach (var x in e.LoaderExceptions.Take(50)) if (x != null) Note(x);
            }
            types += ts.Length;
            foreach (var t in ts) {
                MethodBase[] ms;
                try { ms = t.GetMethods(BindingFlags.DeclaredOnly | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static).Cast<MethodBase>()
                        .Concat(t.GetConstructors(BindingFlags.DeclaredOnly | BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance)).ToArray(); }
                catch (Exception e) { Note(e); continue; }
                foreach (var m in ms) {
                    methods++;
                    if (m.ContainsGenericParameters || t.ContainsGenericParameters) { generic++; continue; }
                    if (m.IsAbstract) continue;
                    try { RuntimeHelpers.PrepareMethod(m.MethodHandle); prepared++; }
                    catch (Exception e) { methodFail++; Note(e); }
                }
            }
        }
        Console.WriteLine($"{System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription} types={types} typeFail={typeFail} methods={methods} prepared={prepared} genericSkipped={generic} methodFail={methodFail}");
        foreach (var kv in why.OrderByDescending(k => k.Value).Take(8)) Console.WriteLine($"  {kv.Value}x {kv.Key}");
    }
}

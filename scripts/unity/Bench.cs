// Game-shaped workloads, kept in the assembly so an interpreter runs them, not the precompiled BCL.
using System; using System.Collections.Generic; using System.Diagnostics; using System.Text;
struct V3 { public float x,y,z; public V3(float a,float b,float c){x=a;y=b;z=c;}
  public static V3 operator+(V3 a,V3 b)=>new V3(a.x+b.x,a.y+b.y,a.z+b.z);
  public static V3 operator*(V3 a,float s)=>new V3(a.x*s,a.y*s,a.z*s);
  public float Dot(V3 o)=>x*o.x+y*o.y+z*o.z; }
struct M4 { public float m00,m01,m02,m03,m10,m11,m12,m13,m20,m21,m22,m23;
  public V3 Mul(V3 v)=>new V3(m00*v.x+m01*v.y+m02*v.z+m03,m10*v.x+m11*v.y+m12*v.z+m13,m20*v.x+m21*v.y+m22*v.z+m23); }
abstract class Comp { public abstract void Tick(float dt); }
class Mover:Comp { public V3 p,v=new V3(1,2,3); public override void Tick(float dt){p=p+v*dt; if(p.x>100)v=v*-1;} }
class Timer:Comp { public float t; public int n; public override void Tick(float dt){t+=dt; if(t>1){t=0;n++;}} }
static class P {
  // A mesh-skinning style loop, like Spine or TextMeshPro rebuilding vertices in C#.
  static float Verts(int count,int rounds){ var src=new V3[count]; var dst=new V3[count];
    for(int i=0;i<count;i++)src[i]=new V3(i,i*.5f,1);
    var m=new M4{m00=1,m11=1,m22=1,m03=2,m13=3,m23=4}; float s=0;
    for(int r=0;r<rounds;r++){ for(int i=0;i<count;i++)dst[i]=m.Mul(src[i]); s+=dst[r%count].x; } return s; }
  // A scene full of MonoBehaviour-style Update calls.
  static float Updates(int count,int rounds){ var l=new List<Comp>(); for(int i=0;i<count;i++)l.Add(i%2==0?new Mover():new Timer());
    for(int r=0;r<rounds;r++) foreach(var c in l)c.Tick(.016f); return ((Timer)l[1]).t; }
  // A VN script step: look up variables, parse a line, build a string.
  static int Script(int rounds){ var vars=new Dictionary<string,int>(); int h=0;
    for(int r=0;r<rounds;r++){ string line="@char Name.Happy pos:"+(r%100)+",50 look:left wait:true"; var parts=line.Split(' ');
      foreach(var p in parts){ int i=p.IndexOf(':'); string k=i>0?p.Substring(0,i):p; vars.TryGetValue(k,out int v); vars[k]=v+1; h+=k.Length; }
      var sb=new StringBuilder(); for(int j=0;j<8;j++)sb.Append((char)('a'+j)); h+=sb.ToString().GetHashCode()&1; } return h; }
  static double Run(string name,Func<double> f){ f(); double best=double.MaxValue; for(int i=0;i<3;i++){ var w=Stopwatch.StartNew(); f(); best=Math.Min(best,w.Elapsed.TotalMilliseconds);} Console.WriteLine($"{name}\t{best:F1}"); return best; }
  static void Main(string[] a){ int k=a.Length>0?int.Parse(a[0]):1;
    Console.WriteLine("runtime\t"+System.Runtime.InteropServices.RuntimeInformation.FrameworkDescription+" dyn="+System.Runtime.CompilerServices.RuntimeFeature.IsDynamicCodeCompiled);
    Run("verts_ms",()=>Verts(10000,20000/k));
    Run("updates_ms",()=>Updates(2000,10000/k));
    Run("script_ms",()=>Script(1_000_000/k)); } }

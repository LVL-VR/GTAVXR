using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Linq;
using System.Text;
using System.Xml;

namespace GtavxrSuite.Content {
    // A parTree element the way tools/mods handles it with Python ElementTree: tag, ordered attributes, the text before the
    // first child, children (comments and tails dropped). The merge rules and their comparisons work on this tree.
    public sealed class Node {
        public string Tag;public string Text;
        public readonly List<KeyValuePair<string,string>> Attrs=new List<KeyValuePair<string,string>>();
        public readonly List<Node> Children=new List<Node>();
        public Node(string tag){Tag=tag;}
        public int Count{get{return Children.Count;}}
        public Node this[int i]{get{return Children[i];}set{Children[i]=value;}}
        public string Get(string name){foreach(var a in Attrs)if(a.Key==name)return a.Value;return null;}
        public void Set(string name,string value){
            for(int i=0;i<Attrs.Count;i++)if(Attrs[i].Key==name){Attrs[i]=new KeyValuePair<string,string>(name,value);return;}
            Attrs.Add(new KeyValuePair<string,string>(name,value));
        }
        public Node Find(string tag){foreach(var c in Children)if(c.Tag==tag)return c;return null;}
        public string FindText(string tag){var c=Find(tag);return c==null?null:(c.Text??"");}
        public Node Add(string tag){var n=new Node(tag);Children.Add(n);return n;}
        public Node Add(string tag,params string[] attrs){var n=Add(tag);for(int i=0;i+1<attrs.Length;i+=2)n.Attrs.Add(new KeyValuePair<string,string>(attrs[i],attrs[i+1]));return n;}
        public IEnumerable<Node> Iter(){yield return this;foreach(var c in Children)foreach(var d in c.Iter())yield return d;}
        public Node DeepCopy(){
            var n=new Node(Tag){Text=Text};n.Attrs.AddRange(Attrs);
            foreach(var c in Children)n.Children.Add(c.DeepCopy());
            return n;
        }

        // ---------------------------------------------------------------- XML in / out
        public static Node Parse(byte[] data){
            var settings=new XmlReaderSettings{DtdProcessing=DtdProcessing.Ignore,XmlResolver=null,IgnoreComments=true,IgnoreProcessingInstructions=true};
            using(var ms=new MemoryStream(data))using(var r=XmlReader.Create(ms,settings)){
                Node root=null;var stack=new Stack<Node>();var started=new Stack<bool>();
                while(r.Read()){
                    switch(r.NodeType){
                        case XmlNodeType.Element:{
                            var n=new Node(r.Name);
                            if(r.HasAttributes){for(int i=0;i<r.AttributeCount;i++){r.MoveToAttribute(i);n.Attrs.Add(new KeyValuePair<string,string>(r.Name,r.Value));}r.MoveToElement();}
                            if(stack.Count>0){stack.Peek().Children.Add(n);started.Pop();started.Push(true);}else root=n;
                            if(!r.IsEmptyElement){stack.Push(n);started.Push(false);}
                            break;
                        }
                        case XmlNodeType.Text:case XmlNodeType.CDATA:case XmlNodeType.Whitespace:case XmlNodeType.SignificantWhitespace:
                            if(stack.Count>0&&!started.Peek()){var t=stack.Peek();t.Text=(t.Text??"")+r.Value;}
                            break;
                        case XmlNodeType.EndElement:stack.Pop();started.Pop();break;
                    }
                }
                if(root==null)throw new InvalidDataException("XML without a root element");
                return root;
            }
        }
        public static Node Load(string path){return Parse(File.ReadAllBytes(path));}
        // ElementTree indent(space='  ') + write(xml_declaration): children on their own lines, leaf text kept.
        public byte[] ToXml(){
            var sb=new StringBuilder("<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
            Write(sb,0);sb.Append('\n');
            return new UTF8Encoding(false).GetBytes(sb.ToString());
        }
        void Write(StringBuilder sb,int depth){
            sb.Append('<').Append(Tag);
            foreach(var a in Attrs)sb.Append(' ').Append(a.Key).Append("=\"").Append(Escape(a.Value,true)).Append('"');
            if(Children.Count==0){
                if(String.IsNullOrEmpty(Text)){sb.Append(" />");return;}
                sb.Append('>').Append(Escape(Text,false)).Append("</").Append(Tag).Append('>');return;
            }
            sb.Append('>');
            string t=Text;if(t!=null&&t.Trim().Length>0)sb.Append(Escape(t,false));
            foreach(var c in Children){sb.Append('\n').Append(' ',2*(depth+1));c.Write(sb,depth+1);}
            sb.Append('\n').Append(' ',2*depth).Append("</").Append(Tag).Append('>');
        }
        static string Escape(string s,bool attr){
            var sb=new StringBuilder(s.Length);
            foreach(char c in s){
                if(c=='&')sb.Append("&amp;");else if(c=='<')sb.Append("&lt;");else if(c=='>')sb.Append("&gt;");
                else if(attr&&c=='"')sb.Append("&quot;");else if(attr&&c=='\n')sb.Append("&#10;");else if(attr&&c=='\r')sb.Append("&#13;");
                else if(attr&&c=='\t')sb.Append("&#09;");else sb.Append(c);
            }
            return sb.ToString();
        }

        // ---------------------------------------------------------------- semantic identity (mods_xml.canon)
        static readonly System.Text.RegularExpressions.Regex Num=new System.Text.RegularExpressions.Regex(@"^[+-]?(\d+\.?\d*|\.\d+)([eE][+-]?\d+)?$");
        public static bool IsNumber(string t){return Num.IsMatch(t);}
        // Python repr(float(t)): one text per double value (so 1, 1.0 and 1.000000 compare equal, -0 stays apart).
        public static string NumKey(string t){
            double d=Double.Parse(t,NumberStyles.Float,CultureInfo.InvariantCulture);
            if(d==0)return 1/d<0?"-0":"0";
            return d.ToString("R",CultureInfo.InvariantCulture);
        }
        public static string NormText(string t,bool foldCase){
            if(t==null)return "";
            var toks=t.Split((char[])null,StringSplitOptions.RemoveEmptyEntries);
            var sb=new StringBuilder();
            for(int i=0;i<toks.Length;i++){
                if(i>0)sb.Append(' ');
                string x=toks[i];
                sb.Append(IsNumber(x)?NumKey(x):foldCase?Fold(x):x);
            }
            return sb.ToString();
        }
        public static string Fold(string s){return s.ToLowerInvariant();}
        public string Canon(bool foldCase){var sb=new StringBuilder();CanonInto(sb,foldCase);return sb.ToString();}
        void CanonInto(StringBuilder sb,bool fold){
            sb.Append('\u0001').Append(Tag).Append('\u0002');
            foreach(var a in Attrs.OrderBy(x=>x.Key,StringComparer.Ordinal))sb.Append(a.Key).Append('\u0003').Append(NormText(a.Value,fold)).Append('\u0004');
            sb.Append('\u0005').Append(NormText(Text,fold)).Append('\u0006');
            foreach(var c in Children)c.CanonInto(sb,fold);
            sb.Append('\u0007');
        }
    }
}

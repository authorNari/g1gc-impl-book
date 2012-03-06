= オブジェクト構造

本章ではオブジェクトのデータ構造を見ていきます。

== oopDescクラス

@<code>{oopDesc}クラスはGC対象となるオブジェクトの抽象的な基底クラスです。
@<code>{oopDesc}クラスを継承したクラスのインスタンスがGC対象のオブジェクトとなります。

@<code>{oopDesc}クラスの継承関係を@<img>{oopDesc_hierarchy}に示します。

//image[oopDesc_hierarchy][oopDescクラスの継承関係]

@<code>{oopDesc}クラスには次のメンバ変数が定義されています。

//source[share/vm/oops/oop.hpp]{
61: class oopDesc {

63:  private:
64:   volatile markOop  _mark;
65:   union _metadata {
66:     wideKlassOop    _klass;
67:     narrowOop       _compressed_klass;
68:   } _metadata;
69: 
71:   static BarrierSet* _bs;
//}

64行目の@<code>{_mark}変数はオブジェクトのヘッダとなる部分です。
@<code>{_mark}変数にはマークスイープGC用のマークはもちろんのこと、その他のオブジェクトに必要な様々な情報が詰め込まれています。

@<code>{oopDesc}は自分のクラスへのポインタを保持しています。
それが65行目に共用体で定義されている@<code>{_metadata}変数です。
この共用体にはほとんどの場合、66行目の@<code>{_klass}変数の値が格納されます。
@<code>{_klass}変数はその名前の通りオブジェクトのクラスへのポインタを格納します。
67行目の@<code>{_compressed_klass}は本章ではGCとは関係ないため特に触れません。

//comment[TODO BarrierSetの話。]

HotspotVMでは@<code>{oopDesc}クラスやその子クラスのインスタンスへのポインタ（@<code>{oopDesc*}）を@<code>{typedef}で別名定義しています。

//source[share/vm/oops/oopsHierarchy.hpp]{
42: typedef class oopDesc*                            oop;
43: typedef class   instanceOopDesc*            instanceOop;
44: typedef class   methodOopDesc*                    methodOop;
45: typedef class   constMethodOopDesc*            constMethodOop;
46: typedef class   methodDataOopDesc*            methodDataOop;
47: typedef class   arrayOopDesc*                    arrayOop;
48: typedef class     objArrayOopDesc*            objArrayOop;
49: typedef class     typeArrayOopDesc*            typeArrayOop;
50: typedef class   constantPoolOopDesc*            constantPoolOop;
51: typedef class   constantPoolCacheOopDesc*   constantPoolCacheOop;
52: typedef class   klassOopDesc*                    klassOop;
53: typedef class   markOopDesc*                    markOop;
54: typedef class   compiledICHolderOopDesc*    compiledICHolderOop;
//}

すべて@<code>{Desc}を取り除いた名前に別名定義されています。
@<code>{oopDesc}の@<code>{Desc}は「Describe（表現）」の略です。
つまり、@<code>{oopDesc}とは@<code>{oop}という実体（オブジェクト）をクラスとして「表現」しているものなのです。
@<code>{oop}等の別名定義は今後、頻繁に登場するため意味も含めてしっかりと抑えておいてください。
本章では@<code>{oopDesc}等のインスタンスを別名定義のルールに従って@<code>{oop}のように呼ぶことにします。

//comment[TODO GCに必要な点はあとで追記していく]

== klassOopDescクラス

@<code>{oopDesc}クラスを継承している@<code>{klassOopDesc}クラスはJava上のクラスを表すクラスです。
つまり、Java上の「java.lang.String」は、VM上では@<code>{klassOopDesc}クラスのインスタンス（@<code>{klassOop}）となります。
クラス名の一部が「@<code>{class}」ではなく「@<code>{klass}」となっているのは、C++の予約語と区別するためです。
このテクニックは多くの言語処理系でよく見かけます。

前の@<hd>{oopDescクラス}の項で説明した通り、全オブジェクトは@<code>{klassOop}を持っています。
@<code>{klassOopDesc}も@<code>{oopDesc}を継承しているため、@<code>{klassOop}を持っています。

@<code>{klassOop}の特徴は内部に@<code>{Klass}クラスのインスタンスを保持しているということです。
実は@<code>{klassOopDesc}クラス自体に情報はほとんどなく、@<code>{klassOop}は内部に@<code>{Klass}クラスのインスタンスを保持するただの箱にすぎません。

== Klassクラス

@<code>{Klass}クラスは名前の通り、型情報を保持しています。
@<code>{Klass}クラスのインスタンスは@<code>{klassOop}の一部として生成されます。

@<code>{Klass}クラスは様々な型情報の抽象的な基底クラスです。
@<code>{Klass}クラスの継承関係を@<img>{klass_hierarchy}に示します。

//image[klass_hierarchy][Klassクラスの継承関係]

@<code>{Klass}クラスの子クラスには、@<code>{oopDesc}の子クラスと対応するクラスが存在します。
@<code>{oopDesc}の子クラスである@<code>{XXDesc}のインスタンスがもつ@<code>{klassOop}内には、@<code>{XXDesc}に対応した@<code>{XXKlass}のインスタンスが格納さているということです。

前の@<hd>{klassOopDescクラス}の項で@<code>{klassOop}はただの箱だといいました。
@<code>{klassOop}はオブジェクトとして@<code>{Klass}やその子クラスを統一的にあつかうためのインタフェースだといえるでしょう。
つまり、外側は@<code>{klassOop}だとしても内部には@<code>{instanceKlass}や@<code>{symbolKlass}等がなどが入っているということです（@<img>{klassOop_box}）。

//image[klassOop_box][klassOopはKlassの箱]

//comment[TODO GCに必要な点はあとで追記していく]

== クラスの関係

では、1つのオブジェクトを例にとって@<code>{oop}と@<code>{Klass}の関係を見ていきましょう。

次のような@<code>{String}クラスのオブジェクトを生成するJavaプログラムがあったとします。

//emlistnum{
String str = new String();
System.out.println(str.getClass()); // => java.lang.String
System.out.println(str.getClass().getClass()); // => java.lang.Class
System.out.println(str.getClass().getClass().getClass()); // => java.lang.Class
//}

その場合、@<code>{str}変数には@<code>{String}クラスのオブジェクトが格納されます。
この時、HotspotVM上での@<code>{oop}と@<code>{Klass}の関係は@<img>{oop_by_string}のようになっています。

//image[oop_by_string][Stringオブジェクトのoop]

@<code>{instanceOop}はJava上のインスタンスと同じ意味を持ちます。
つまり、「new String()」をVMで評価すると@<code>{instanceOop}が1つ生成されます。

@<code>{instanceOop}のクラスはもちろん@<code>{klassOop}です。
そして、@<code>{klassOop}の中には@<code>{instanceKlass}のインスタンスが格納されています。
この@<code>{klassOop}はJava上の@<code>{String}クラスと対応しています。

次に、Java上のStringクラスの@<code>{klassOop}のクラスは同じく@<code>{klassOop}です。
この@<code>{klassOop}の中には@<code>{instanceKlassKlass}のインスタンスが格納されています。

@<code>{instanceKlassKlass}は@<code>{instanceKlass}のクラスです。
@<code>{instanceKlassKlass}をもつ@<code>{klassOop}のクラスは自分自身であるため、@<code>{instanceOop}から続くクラスの連鎖を止める役割を持っています。
Javaプログラムを見ると3行目の@<code>{getClass()}メソッドの結果と、4行目の@<code>{getClass()}メソッドの結果が同じ値になっています。
これはクラスの連鎖が@<code>{instanceKlassKlass}のところでループしているためです。

== oopDescクラスに仮想関数を定義してはいけない

@<code>{oopDesc}クラスにはC++の仮想関数（virtualfunction）@<fn>{virtual_function}を定義してはいけない決まりになっています。

その理由はクラスに仮想関数を定義するとC++のコンパイラがそのクラスのインスタンスに仮想関数テーブル@<fn>{vtable}へのポインタを付けてしまうからです。
すべてのオブジェクトに1ワード確保されては困りものです。そのため、@<code>{oopDesc}クラスにはC++の仮想関数を定義できないルールとなっています。

もし、仮想関数を使って子クラス毎に違う振る舞いをするメンバ関数を定義したい場合は@<code>{oopDesc}ではなく、対応する@<code>{Klass}の方に仮想関数を定義します。

次にその一部を示します。
ここでは自分がJava上でどのような意味をもつオブジェクトかを判断する仮想関数が定義されています。

//source[share/vm/oops/klass.hpp]{
172: class Klass : public Klass_vtbl {

// Java上の配列か？
582:   virtual bool oop_is_array()               const { return false; }
//}

上記のメンバ関数は@<code>{Klass}を継承するクラスで次のように再定義されます。

//source[shara/vm/oops/arrayKlass.hpp]{
35: class arrayKlass: public Klass {
47:   bool oop_is_array() const { return true; }
//}

そして、@<code>{oopDesc}クラスでは@<code>{Klass}クラスの仮想関数を呼び出します。

//source[share/vm/oops/oop.inline.hpp]{
139: inline Klass* oopDesc::blueprint() const {
       return klass()->klass_part(); }
146: inline bool oopDesc::is_array() const {
       return blueprint()->oop_is_array(); }
//}

139行目の@<code>{blueprint()}は@<code>{klassOop}の中から@<code>{Klass}のインスタンスを取り出すメンバ関数です。
146行目では@<code>{Klass}の仮想関数で定義されたメンバ関数を呼び出しています。
@<code>{oop}の@<code>{is_array()}を呼び出しても、対応した@<code>{Klass}の@<code>{oop_is_array()}が応答し、@<code>{false}が戻りますが、@<code>{arrayOop}の@<code>{is_array()}を呼び出すと、対応した@<code>{arrayKlass}の@<code>{oop_is_array()}が応答し、@<code>{true}が戻ります。

@<code>{Klass}クラスに仮想関数を定義するため、@<code>{klassOop}には仮想関数テーブルへのポインタが付いてしまいますが、Java上のクラスは全体量が少ないため、それほどメモリを消費しません。

//footnote[virtual_function][仮想関数：子クラスで再定義可能な関数のこと。C++上の文法でメンバ関数に virtual を付けると仮想関数となる]
//footnote[vtable][仮想関数テーブル：実行時に呼び出すメンバ関数の情報を格納している]


= オブジェクト構造

本章ではVMヒープに対して割り当てられるオブジェクトのデータ構造を見ていきます。
割り当てられたオブジェクトは当然ながらGCの対象となります。

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
//}

64行目の@<code>{_mark}変数はオブジェクトのヘッダとなる部分です。
@<code>{_mark}変数にはマークスイープGC用のマークはもちろんのこと、その他のオブジェクトに必要な様々な情報が詰め込まれています。

@<code>{oopDesc}は自分のクラスへのポインタを保持しています。
それが65行目に共用体で定義されている@<code>{_metadata}変数です。
この共用体にはほとんどの場合、66行目の@<code>{_klass}変数の値が格納されます。
@<code>{_klass}変数はその名前の通りオブジェクトのクラスへのポインタを格納します。
67行目の@<code>{_compressed_klass}はGCとは関係ないため本書では特に触れません。

HotspotVMでは、@<code>{oopDesc}インスタンスのポインタ（@<code>{oopDesc*}）などを@<code>{typedef}で別名定義しています。

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
本章では@<code>{oopDesc}等のインスタンスを別名定義のルールに従って@<code>{oop}のように呼ぶことにします。

== klassOopDescクラス

@<code>{oopDesc}クラスを継承している@<code>{klassOopDesc}クラスはJava上のクラスを表すクラスです。
つまり、Java上の「java.lang.String」は、VM上では@<code>{klassOopDesc}クラスのインスタンス（@<code>{klassOop}）となります。
クラス名の一部が「@<code>{class}」ではなく「@<code>{klass}」となっているのは、C++の予約語と区別するためです。
このテクニックは多くの言語処理系でよく見かけます。

前の@<hd>{oopDescクラス}の項で説明した通り、全オブジェクトは@<code>{klassOop}を持っています。
また@<code>{klassOopDesc}自体も@<code>{oopDesc}を継承しているため、@<code>{klassOop}をメンバ変数として持っています。

@<code>{klassOop}の特徴は内部に@<code>{Klass}クラスのインスタンスを保持しているということです。
実は@<code>{klassOopDesc}クラス自体に情報はほとんどなく、@<code>{klassOop}は内部に@<code>{Klass}インスタンスを保持するただの箱にすぎません。

== Klassクラス

@<code>{Klass}クラスは名前の通り、型情報を保持しています。
@<code>{Klass}のインスタンスは@<code>{klassOop}の一部として生成されます。

@<code>{Klass}は様々な型情報の抽象的な基底クラスです。
@<code>{Klass}の継承関係を@<img>{klass_hierarchy}に示します。

//image[klass_hierarchy][Klassクラスの継承関係]

@<code>{Klass}の子クラスには、@<code>{oopDesc}の子クラスと対応するクラスが存在します。
そのような@<code>{XXDesc}のインスタンスには、@<code>{XXDesc}に対応した@<code>{XXKlass}を保持する@<code>{klassOop}が格納されます。

前の@<hd>{klassOopDescクラス}の項で@<code>{klassOop}はただの箱だといいました。
@<code>{klassOop}はオブジェクトとして@<code>{Klass}やその子クラスを統一的にあつかうためのインタフェースだといえるでしょう。
つまり、外側は@<code>{klassOop}だとしても内部には@<code>{instanceKlass}や@<code>{symbolKlass}などが入っているということです（@<img>{klassOop_box}）。

//image[klassOop_box][klassOopはKlassの箱]

== クラスの関係

では、1つのオブジェクトを例にとって@<code>{oop}と@<code>{Klass}の関係を具体的に見ていきましょう。

次のような@<code>{String}クラスのオブジェクトを生成するJavaプログラムがあったとします。

//listnum[new_string][Stringオブジェクトを生成するJavaプログラム]{
String str = new String();
System.out.println(str.getClass()); // => java.lang.String
System.out.println(str.getClass().getClass()); // => java.lang.Class
System.out.println(str.getClass().getClass().getClass()); // => java.lang.Class
//}

その場合、@<code>{str}変数には@<code>{String}クラスのオブジェクトが格納されます。
この時、HotspotVM上での@<code>{oop}と@<code>{Klass}の関係は@<img>{oop_by_string}のようになっています。

//image[oop_by_string][Stringオブジェクトのoop]

@<code>{instanceOop}はJava上のインスタンスへの参照と同じ意味を持ちます。
@<img>{oop_by_string}左端の@<code>{instanceOop}は、「new String()」の評価時に生成された@<code>{instanceOopDesc}のインスタンスへのポインタを示しています。

@<code>{instanceOop}は自身の@<code>{_klass}変数（オブジェクトのクラスを示す変数）に@<code>{klassOop}をもちます。
そして、その@<code>{klassOop}の中には@<code>{instanceKlass}のインスタンスが格納されます。
@<img>{oop_by_string}中央の@<code>{klassOop}は、@<list>{new_string}の2行目で示したJava上の@<code>{String}クラスと対応しています。

次に、図中央の@<code>{klassOop}（＝Java上のStringクラス）は内部にさらに別の@<code>{klassOop}を持ちます。
この@<code>{klassOop}の中には@<code>{instanceKlassKlass}のインスタンスが格納されています。
@<img>{oop_by_string}右端の@<code>{klassOop}が上記を表しており、@<list>{new_string}の3行目で示したJava上の@<code>{Class}クラスと対応しています。

@<code>{instanceKlassKlass}は@<code>{instanceKlass}のクラスを示します。
@<code>{instanceKlassKlass}をもつ@<code>{klassOop}は自分自身を@<code>{_klass}にもち、@<code>{instanceOop}から続くクラスの連鎖を収束させる役割を持っています。
@<list>{new_string}を見ると3行目の@<code>{getClass()}メソッドの結果と、4行目の@<code>{getClass()}メソッドの結果が同じ値になっています。
これはクラスの連鎖が@<code>{instanceKlassKlass}のところでループしているためです。

== oopDescクラスに仮想関数を定義してはいけない

@<code>{oopDesc}クラスにはC++の仮想関数（virtual function）@<fn>{virtual_function}を定義してはいけない決まりになっています。

クラスに仮想関数を定義するとC++のコンパイラがそのクラスのインスタンスに仮想関数テーブル@<fn>{vtable}へのポインタを勝手に付けてしまいます。
もし@<code>{oopDesc}に仮想関数を定義するとすべてのオブジェクトに対して1ワードが確保されてしまいます。
これは空間効率が悪いので、@<code>{oopDesc}クラスにはC++の仮想関数を定義できないルールとなっています。

もし、仮想関数を使って子クラス毎に違う振る舞いをするメンバ関数を定義したい場合は@<code>{oopDesc}ではなく、対応する@<code>{Klass}の方に仮想関数を定義しなければなりません。

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

そして、@<code>{oopDesc}クラスでは@<code>{Klass}の仮想関数を呼び出します。

//source[share/vm/oops/oop.inline.hpp]{
139: inline Klass* oopDesc::blueprint() const {
       return klass()->klass_part(); }
146: inline bool oopDesc::is_array() const {
       return blueprint()->oop_is_array(); }
//}

139行目の@<code>{blueprint()}は@<code>{klassOop}の中から@<code>{Klass}のインスタンスを取り出すメンバ関数です。
146行目では@<code>{Klass}の仮想関数で定義されたメンバ関数を呼び出しています。
@<code>{oop}の@<code>{is_array()}を呼び出しても、対応した@<code>{Klass}の@<code>{oop_is_array()}が応答し、@<code>{false}が戻りますが、@<code>{arrayOop}の@<code>{is_array()}を呼び出すと、対応した@<code>{arrayKlass}の@<code>{oop_is_array()}が応答し、@<code>{true}が戻ります。

@<code>{Klass}に仮想関数を定義するため、@<code>{klassOop}には仮想関数テーブルへのポインタが付いてしまいますが、Java上のクラスはオブジェクトよりも全体量が少ないため問題になりません。

//footnote[virtual_function][仮想関数：子クラスで再定義可能な関数のこと。C++上の文法でメンバ関数に virtual を付けると仮想関数となる]
//footnote[vtable][仮想関数テーブル：実行時に呼び出すメンバ関数の情報を格納している]

== オブジェクトのヘッダ
@<hd>{oopDescクラス}で少し取り上げたオブジェクトのヘッダについてもう少し説明しておきましょう。
オブジェクトのヘッダは@<code>{markOopDesc}クラスで表現されます。

ヘッダ内の主な情報として次のものが詰め込まれます。

 * オブジェクトのハッシュ値
 * 年齢（世代別GCに利用）
 * ロックフラグ

=== トリッキーなmarkOopDesc
ヘッダを表す@<code>{markOopDesc}クラスはかなりトリッキーなコードになっています。
著者はC++にあまり馴染みがないので「こんな書き方もできるのか…」と驚かされました。

@<code>{markOopDesc}は1ワードのデータだけをヘッダとして利用するクラスです。
@<code>{markOopDesc}の利用イメージを次に示します。

//listnum[mark_oop_desc_01][@<code>{markOopDesc}の利用イメージ]{
markOopDesc* header;
uintptr_t some_word = 1;

header = (markOopDesc*)some_word;
header->is_marked();  // マーク状態を調べる
//}

1行目で@<code>{markOopDesc*}のローカル変数を定義し、2行目では@<code>{uintptr_t}、つまり1ワードのデータをローカル変数で定義しています。

4行目でそれを@<code>{markOopDesc*}にキャストし、5行目で関数呼び出し…。
さて、@<code>{some_word}は@<code>{1}だったはずです。
つまり、5行目では@<code>{1}というアドレス上のデータをインスタンスとみなして関数呼び出ししているのですから、SEGVが起こってもおかしくないような…？

実は@<code>{markOopDesc}クラス自体はインスタンスを生成せず、自身のアドレス（@<code>{this}）しか利用しないように実装されています。
自身のアドレスをヘッダ用の情報として利用するクラスなのです。

//source[share/vm/oops/markOop.hpp]{
104: class markOopDesc: public oopDesc {
105:  private:
107:   uintptr_t value() const { return (uintptr_t) this; }

221:   bool is_marked()   const {
222:     return (mask_bits(value(), lock_mask_in_place) == marked_value);
223:   }
//}

107行目の@<code>{value()}という@<code>{this}を返すものをベースにさまざまなメンバ関数が実装されています。
利用例として221〜223行目に@<code>{is_marked()}というマーク済みかどうかを返すメンバ関数をみてみましょう。
222行目では@<code>{value()}で得た1ワードデータをマスクし、マークビットが立っているかどうかを判断して結果を返しています。

さて、104行目で@<code>{markOopDesc}は@<code>{oopDesc}クラスを一応継承していますが、これはまったく利用しません。
@<code>{oopDesc}からいくらかのメンバ変数も継承しますが、@<code>{markOopDesc}はインスタンスを持たないのでこれらも使うことができません。
じゃあ、なぜこんな余計なクラスを継承しているのか、という話ですが、きちんとコメントが書いてありました。

//source[share/vm/oops/markOop.hpp]{
32: // Note that the mark is not a real oop but just a word.
33: // It is placed in the oop hierarchy for historical reasons.
    // （訳）
    // markはほんとうのoopではなくただのワードであることに注意してください。
    // これがoopの継承関係にいるのは歴史的な理由によるものです。
//}

なるほど。うん、しょうがないか。歴史的な理由ならしょうがないですね。

個人的にはこんな複雑なことはせずに、単純に1ワードのメンバ変数をもつようなクラスを定義すればいいのではと思います。
C++だとコンパイラが余計なデータ領域を確保したりすることがあるので嫌なのでしょうか（vtableなど）。
でも、これは読みづらいですよね…。

=== フォワーディングポインタ
オブジェクトヘッダの使い方の実例として、コピーGCに利用されるフォワーディングポインタとしての利用方法を見てみましょう。

以下にG1GCのオブジェクトをコピーするメンバ関数を示します。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
4369: oop G1ParCopyHelper::copy_to_survivor_space(oop old) {

4370:   size_t    word_sz = old->size();

4382:   HeapWord* obj_ptr = _par_scan_state->allocate(alloc_purpose, word_sz);
4383:   oop       obj     = oop(obj_ptr);

4395:   oop forward_ptr = old->forward_to_atomic(obj);
4396:   if (forward_ptr == NULL) {
          // オブジェクトのコピー
4397:     Copy::aligned_disjoint_words((HeapWord*) old, obj_ptr, word_sz);

4457:   }
4458:   return obj;
4459: }
//}

引数にはコピー元のオブジェクトへのポインタ（@<code>{old}）を受け取ります。
4370行目でオブジェクトのサイズを取得し、サイズ分のオブジェクトを4382行目で新たに割り当てます。
4383行目で割り当てた領域に対するアドレスを@<code>{oop}にキャストします。

4383行目の@<code>{oop(p)}の部分は若干説明が必要でしょう。
C++では関数呼び出しと同様の構文で明示的な型変換をおこなうことができます。
キャストと明示的な型変換は複数の引数が受け取れる箇所が異なります。
キャストの場合は引数が実質1つしか受け取れませんが、明示的な型変換は複数の引数を受け取れます。
ただ、@<code>{oop(p)}の場合は引数を1つしか受け取っていませんので、@<code>{(oop)p}と同じことだと考えればいいでしょう。

4395行目の@<code>{forward_to_atomic()}がフォワーディングポインタを作成するメンバ関数です。
このメンバ関数は並列に動く可能性があり、途中で他のスレッドに割り込まれて先にコピーされてしまった場合は@<code>{NULL}を返します。
無事、フォワーディングポインタを設定できたら、4397行目で実際にオブジェクトの内容をコピーし、4458行目でコピー先のアドレスを返します。

では、@<code>{forward_to_atomic()}メンバ関数の中身を見てみましょう。

//source[share/vm/oops/oop.pcgc.inline.hpp]{
76: inline oop oopDesc::forward_to_atomic(oop p) {

79:   markOop oldMark = mark();
80:   markOop forwardPtrMark = markOopDesc::encode_pointer_as_mark(p);
81:   markOop curMark;

86:   while (!oldMark->is_marked()) {
87:     curMark = (markOop)Atomic::cmpxchg_ptr(
                             forwardPtrMark, &_mark, oldMark);

89:     if (curMark == oldMark) {
90:       return NULL;
91:     }

95:     oldMark = curMark;
96:   }
97:   return forwardee();
98: }
//}

79行目でコピー元のオブジェクトのヘッダをローカル変数の@<code>{oldMark}に格納します。
次に80行目でコピー先のアドレスをフォワーディングポインタにエンコードします。
この静的メンバ関数の中身は後述します。

その後、86〜96行目で、CAS命令を利用して不可分にフォワーディングポインタをコピー元のオブジェクトの@<code>{_mark}に書き込みます。
97行目の@<code>{forwardee()}でフォワーディングポインタをデコードし、呼び出し元に返します。
この関数の役割は後述するマークビットを外すだけしかありません。

以下にフォワーディングポインタをエンコードする@<code>{encode_pointer_as_mark()}を示します。

//source[share/vm/oops/markOop.hpp]{
363:   inline static markOop encode_pointer_as_mark(void* p) {
         return markOop(p)->set_marked();
       }
//}

受け取ったポインタを@<code>{markOop}にキャストして、@<code>{set_marked()}を呼び出しているだけですね。

//source[share/vm/oops/markOop.hpp]{
158:   enum { locked_value             = 0,
              // ...
161:          marked_value             = 3,
              // ...
163:   };

333:   markOop set_marked()   {
         return markOop((value() & ~lock_mask_in_place) | marked_value);
       }
//}

@<code>{set_marked()}は下位2ビットを@<code>{1}にする（マークビットを立てる）メンバ関数です。
オブジェクトアドレスの下位2ビットは必ず@<code>{0}になるようにアラインメントされることを利用したハックですね。

上記のマークによって、そのオブジェクトがすでにコピーされているか判断することが出来ます。

//source[share/vm/oops/oop.inline.hpp]{
641: inline bool oopDesc::is_forwarded() const {
644:   return mark()->is_marked();
645: }
//}

G1GCやその他のコピーGCでは上記の@<code>{is_forwarded()}を確認し、マークが付いているものは再度コピーしないようにします。

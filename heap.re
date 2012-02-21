= ヒープ構造

この章ではHotspotVMはどのようなヒープ構造をもっているか、その全体像を探っていきます。

== VMヒープ

HotspotVMのVMヒープは大きく次の2つに分かれます。

 1. 選択したGC用のメモリ領域
 2. パーマネント（Permanent）領域

//image[vm_heap][VMヒープの全体像]

Java（HotspotVM）の利用者はGCアルゴリズムを選択することができました。
HotspotVMは言語利用者によってGCアルゴリズムが選択されると、そのGC用の構造を持つメモリ領域を作成します。
それが1.です。このメモリ領域に選択したGCの対象オブジェクトを割り当てます。

2.のパーマネント領域のパーマネント（Permanent）には「永続的な」という意味があります。
この名前の通り、パーマネント領域には型情報（@<code>{klassOop}）やメソッド情報（@<code>{methodOop}）等の長生きするオブジェクトが割り当てられます。
パーマネント領域はGCアルゴリズムが変更されてもほぼ同じ構造でVMヒープの一部として確保されます。

== CHeapObjクラス

VMヒープの実装を見る前に@<code>{CHeapObj}というクラスについて説明しておきましょう。
HotspotVM内の多くのクラスはこの@<code>{CHeapObj}クラスを継承しています。

//comment[TODO: 役割が書いていない。Cのヒープ領域から確保されるオブジェクト。]

@<code>{CHeapObj}クラスの特殊な点はオペレータの@<code>{new}と@<code>{delete}を書き換え、C++の通常のアロケーションにデバッグ処理を追加してるところです。
このデバッグ処理は開発時にのみ使用されます。

@<code>{CHeapObj}クラスにはいくつかのデバッグ処理が実装されていますが、その中で「メモリ破壊検知」について簡単に見てみましょう。

デバッグ時、@<code>{CHeapObj}クラス（または継承先クラス）のインスタンスを生成する際に、わざと余分にアロケーションします。
アロケーションされたメモリのイメージを@<img>{cheap_obj_debug_by_new}に示します。

//image[cheap_obj_debug_by_new][デバッグ時のCHeapObjインスタンス]

余分にアロケーションしたメモリ領域を@<img>{cheap_obj_debug_by_new}内の「メモリ破壊検知領域」として使います。
メモリ破壊検知領域には@<code>{0xAB}という値を書き込んでおきます。
@<code>{CHeapObj}クラスのインスタンスとして@<img>{cheap_obj_debug_by_new}内の真ん中のメモリ領域を使用します。

そして、@<code>{CHeapObj}クラスのインスタンスの@<code>{delete}時に「メモリ破壊検知領域」が@<code>{0xAB}のままであるかをチェックします。
もし、書き換わっていれば、@<code>{CHeapObj}クラスのインスタンスの範囲を超えたメモリ領域に書き込みが行われたということです。
これはメモリ破壊が行われた証拠となりますので、発見次第エラーを出力し、終了します。

== CollectedHeapクラス

//comment[TODO: あとで削除]

本章ではG1GC用のVMヒープである@<code>{g1CollectedHeap}クラスについてのみ説明します。

== G1GCヒープ

G1GCのヒープは一定のサイズのリージョンに区切られています。
ここではG1GCヒープがリージョンをどのように保持しているかを見ていきましょう。

G1GCヒープはそのサイズ分のメモリ領域を一度に確保します。
そのため、G1GCヒープ内に確保されるリージョンは連続したアドレスになります。

そして、リージョンには、それぞれを管理する@<code>{HeapRegion}クラスのインスタンスが存在します。
これを@<code>{HeapRegion}と呼ぶことにします。

@<code>{HeapRegion}は@<code>{_bottom}、@<code>{_end}メンバ変数をもち、それぞれG1GCヒープ内のリージョンの先頭アドレス、終端アドレスを格納しています。

//image[heap_region][HeapRegion]

次に列挙するのは@<code>{g1CollectedHeap}クラスの主要な3つのメンバ変数とその役割です。

 * @<code>{_hrs} - すべての@<code>{HeapRegion}を配列によって保持
 * @<code>{_young_list} - 新世代の@<code>{HeapRegion}リスト
 * @<code>{_free_region_list} - 未使用の@<code>{HeapRegion}リスト

//image[g1gc_heap][G1GCヒープの構造]

@<code>{g1CollectedHeap}クラスは@<code>{HeapRegionSeq}クラスのインスタンスへのポインタを格納する@<code>{_hrs}メンバ変数を持ちます。
@<code>{HeapRegionSeq}クラスの@<code>{_regions}メンバ変数という配列には@<code>{HeapRegion}クラスのインスタンス（以下、@<code>{HeapRegion}）のアドレスが格納されています。

@<code>{HeapRegion}は@<code>{g1CollectedHeap}クラスから伸びる@<code>{_young_list}、@<code>{_free_region_list}によって片方向リストでつながれています。

新世代の@<code>{HeapRegion}は@<code>{_young_list}につながれています。空のリージョンと対応する@<code>{HeapRegion}はフリーリージョンリスト（@<code>{_free_region_list}）によってつながれています。そして、旧世代の@<code>{HeapRegion}は何のリンクにもつながれていません。

//comment[TODO: GC用の追記をするかもしれない]
//comment[TODO: 旧世代は？]

== HeapRegionSeqクラス

@<code>{HeapRegionSeq}クラスはHotspotVMが独自に実装している@<code>{GrowableArray}という配列を表現するクラスをラップする形で定義されています。

//source[share/vm/gc_implementation/g1/heapRegionSeq.hpp]{
34: class HeapRegionSeq: public CHeapObj {
35: 

38:   GrowableArray<HeapRegion*> _regions;

56:  public:

63:   void insert(HeapRegion* hr);

70:   HeapRegion* at(size_t i) { return _regions.at((int)i); }

114: };
//}

38行目の@<code>{_regions}メンバ変数がリージョンに対応するを保持する配列（@<code>{GrowableArray}クラス）です。

@<code>{GrowableArray}クラスは通常の配列と違い、要素を追加する際に配列を拡張する処理が実装されています。
名前の通り、増大可能な（Growable）配列なのです。

@<code>{_regions}メンバ変数の配列内の@<code>{HeapRegion}はリージョンのアドレスによって昇順にソートされています。
つまり、インデックス0とインデックス1に格納されている@<code>{HeapRegion}は、VMヒープ上で隣り合ったリージョンを管理しているということです。

63行目の@<code>{insert()}メンバ関数で@<code>{_regions}に新しいリージョンのアドレスを追加します。この@<code>{insert()}関数内で配列のソート処理を行います。

70行目の@<code>{at()}メンバ関数は指定したインデックスのリージョンを返します。

== HeapRegionクラス

リージョンを管理する@<code>{HeapRegion}クラスをさらに詳しく見ていきましょう。

@<code>{HeapRegion}クラスの継承図を@<img>{heap_region_hierarchy}に示します。

//image[heap_region_hierarchy][HeapRegion継承図]

継承関係が深いですが、そのすべてを覚える必要はありません。
「様々なクラスから機能を受け継いでいる」ということがわかればOKです。

@<code>{HeapRegion}クラスには@<code>{_bottom}、@<code>{_top}、@<code>{_end}という3つのローカル変数があります。
それぞれの意味は次の通りです。

 * _bottom - リージョンの先頭アドレス
 * _top - リージョン内のチャンク先頭アドレス
 * _end - リージョンの終端アドレス

この3つのローカル変数は@<code>{Space}クラスに定義されています。
もっともよく登場するローカル変数ですので、きちんと頭に入れておいてください。

さらに、@<code>{HeapRegion}クラスには3つの片方向リスト用のメンバ変数が定義されています。

 1. @<code>{_next_young_region}
 2. @<code>{_next_dirty_cards_region}
 3. @<code>{_next_in_special_set}

1.は名前の通り、次の新世代リージョンを指します。
//comment[2.については@<chap>{g1gc}の章で詳しく説明します。]

もっとも重要なのは3.の@<code>{_next_in_special_set}メンバ変数です。
このメンバ変数はリージョンが所属する集合によって意味の違う様々なリージョンを指します。
具体的に言えば、リージョンがフリーリージョンリストに所属するときには、次の空リージョンがつながれ、GC対象のリストに所属するときには、次のGC対象である使用中のリージョンがつながれます。
@<code>{_next_in_special_set}メンバ変数は用途によって様々な使い方がされるということを覚えておいてください。

また、@<code>{_next_in_special_set}メンバ変数にリージョンをつなぐとき、@<code>{_next_in_special_set}メンバ変数が何の用途に使われているかを覚えておくため、「このリージョンはこの集合に所属しています」というフラグを立てておきます。

フラグに使用するメンバ変数は次の通りです。これらのフラグはすべて@<code>{bool}型です。

  * _in_collection_set - GC対象のリージョン
  * _is_on_unclean_list - 空のリージョン（0クリアがまだ）
  * _is_on_free_list - 空のリージョン（0クリアが済み）
  * _is_gc_alloc_region - GC時にオブジェクトを退避するリージョン

//comment[TODO:意味]

ここではすべてを覚える必要はありません。
このようなものでリージョンの所属を判断しているのかという点が理解できれば十分です。

== パーマネント領域

G1GCのパーマネント領域は@<code>{CompactingPermGenGen}クラスによって管理されています。

@<code>{g1CollectedHeap}クラスの@<code>{_perm_gen}というメンバ変数に@<code>{CompactingPermGenGen}クラスのインスタンスへのポインタが格納されます。

パーマネント領域はG1GCではなく、マークコンパクトGCの対象となります。そのため、本章では詳細な説明は行いません。

= ヒープ構造

本章ではG1GCのVMヒープ構造について述べます。

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

=== VMヒープクラスの初期化

すべてのVMヒープクラスは@<code>{CollectedHeap}クラスを継承しています。

//source[share/vm/gc_interface/collectedHeap.hpp]{
53: class CollectedHeap : public CHeapObj {

286:   virtual bool is_permanent(const void *p) const = 0;

323:   inline static oop obj_allocate(KlassHandle klass, int size, TRAPS);

497:   virtual void collect(GCCause::Cause cause) = 0;
//}

@<code>{CollectedHeap}は@<code>{CHeapObj}を継承しており、上記のようにさまざまインタフェースを定義します。

適切なVMヒープクラスは@<code>{Universe::initialize_heap()}で選ばれます。

//source[share/vm/memory/universe.cpp]{
882: jint Universe::initialize_heap() {
883: 
884:   if (UseParallelGC) {

886:     Universe::_collectedHeap = new ParallelScavengeHeap();

891:   } else if (UseG1GC) {

893:     G1CollectorPolicy* g1p = new G1CollectorPolicy_BestRegionsFirst();
894:     G1CollectedHeap* g1h = new G1CollectedHeap(g1p);
895:     Universe::_collectedHeap = g1h;

900:   } else {

901:     GenCollectorPolicy *gc_policy;

         /* 省略: 適切なCollectorPolicyを選ぶ */

919:     Universe::_collectedHeap = new GenCollectedHeap(gc_policy);
920:   }
921: 
922:   jint status = Universe::heap()->initialize();

       ...
//}

GCアルゴリズムとVMヒープクラスの対応については@<hd>{abstract|CollectorPolicyクラス}ですでに述べました。
ここでは、適切なVMヒープクラスのインスタンスが生成され、最終的に@<code>{initialize()}を呼び出す点と、@<code>{Universe::_collectedHeap}に格納される点を抑えておいてください。

次に示す通り、@<code>{Universe}クラスは@<code>{AllStatic}クラスを継承したクラスです。

//source[share/vm/memory/universe.hpp]{
113: class Universe: AllStatic {

201:   static CollectedHeap* _collectedHeap;

346:   static CollectedHeap* heap() { return _collectedHeap; }
//}

@<code>{Universe::heap()}を呼び出すことで、@<code>{Universe::initialize_heap()}で選択した適切なVMヒープのインスタンスを取得できます。

== G1GCヒープ

G1GCのヒープは『アルゴリズム編 1.2 ヒープ構造』で示したとおり、一定のサイズのリージョンに区切られています。
ここではG1GCヒープがリージョンをどのように保持しているかを見ていきましょう。

=== G1CollectedHeapクラス

@<code>{G1CollectedHeap}クラスは非常に多くの役割を担っていますが、一度に説明しても混乱するだけですので、ここでは@<code>{G1CollectedHeap}クラスの主要な3つのメンバ変数の紹介に留めておきます。

 * @<code>{_hrs} - すべての@<code>{HeapRegion}を配列によって保持
 * @<code>{_young_list} - 新世代の@<code>{HeapRegion}リスト
 * @<code>{_free_region_list} - 空き@<code>{HeapRegion}リスト

//image[g1gc_heap][G1GCヒープの構造]

各リージョンは@<code>{HeapRegion}というクラスで管理されています。
@<code>{G1CollectedHeap}クラスは@<code>{HeapRegionSeq}インスタンスへのポインタを格納する@<code>{_hrs}メンバ変数を持っています。
@<code>{HeapRegionSeq}の@<code>{_regions}メンバ変数には、G1GCヒープのすべてのリージョンに対応する@<code>{HeapRegion}のアドレスが配列によって格納されています。

@<code>{HeapRegion}は@<code>{G1CollectedHeap}クラスの@<code>{_young_list}、@<code>{_free_region_list}によって片方向リストでつながれています。

新世代の@<code>{HeapRegion}は@<code>{_young_list}につながれています。
空のリージョンと対応する@<code>{HeapRegion}は空きリージョンリスト（@<code>{_free_region_list}）によってつながれています。
そして、旧世代の@<code>{HeapRegion}は何のリンクにもつながれていません。

=== HeapRegionクラス

@<code>{HeapRegion}は@<code>{_bottom}、@<code>{_end}メンバ変数をもち、それぞれリージョンの先頭アドレス、終端アドレスを格納しています。

//image[heap_region][HeapRegion]

@<code>{HeapRegion}クラスの継承図を@<img>{heap_region_hierarchy}に示します。

//image[heap_region_hierarchy][HeapRegion継承図]

継承関係が深いですが、そのすべてを覚える必要はありません。
「様々なクラスから機能を受け継いでいる」ということがわかればOKです。

@<code>{HeapRegion}クラスは@<code>{_bottom}・@<code>{_end}の他に@<code>{_top}というメンバ変数を持ちます。
@<code>{_top}はリージョン内の空きメモリ領域の先頭アドレスを保持します。
@<code>{_bottom}・@<code>{_end}・@<code>{_top}は@<code>{Space}クラスに定義されており、よく利用されるメンバ変数ですので、しっかり覚えておきましょう。

@<code>{HeapRegion}クラスには次の片方向リスト用のメンバ変数が定義されています。

 1. @<code>{_next_young_region}
 2. @<code>{_next_in_special_set}

1.は名前の通り、次の新世代リージョンを指します。

2.の@<code>{_next_in_special_set}メンバ変数はリージョンが所属する集合によって意味の違う様々なリージョンを指します。
具体的に言えば、リージョンがフリーリージョンリストに所属するときには、次の空リージョンがつながれ、回収集合のリストに所属するときには、次のGC対象である使用中のリージョンがつながれます。
@<code>{_next_in_special_set}メンバ変数は用途によって様々な使い方がされるということを覚えておいてください。

また、@<code>{_next_in_special_set}メンバ変数にリージョンをつなぐとき、@<code>{_next_in_special_set}メンバ変数が何の用途に使われているかを覚えておくため、「このリージョンはこの集合に所属しています」というフラグを立てておきます。

フラグに使用する主要なメンバ変数は次の通りです。
これらのフラグはすべて@<code>{bool}型です。

  * @<code>{_in_collection_set} - 回収集合内のリージョン
  * @<code>{_is_gc_alloc_region} - 前回の退避後にオブジェクトが割り当てられたリージョン

=== HeapRegionSeqクラス

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

@<code>{_regions}に格納される@<code>{HeapRegion}の配列は、リージョンの先頭アドレスの昇順にソートされた状態を保ちます。
63行目の@<code>{insert()}メンバ関数で@<code>{_regions}に新しいリージョンのアドレスを追加します。
この@<code>{insert()}で配列のソートをおこないます。

70行目の@<code>{at()}メンバ関数は指定したインデックスのリージョンを返します。

== パーマネント領域

G1GCのパーマネント領域は@<code>{CompactingPermGenGen}クラスによって管理されています。

@<code>{g1CollectedHeap}クラスの@<code>{_perm_gen}というメンバ変数に@<code>{CompactingPermGenGen}クラスのインスタンスへのポインタが格納されます。

パーマネント領域はG1GCではなく、マークコンパクトGCの対象となります。そのため、本章では詳細な説明は行いません。

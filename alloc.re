= アロケータ

この章ではHotspotVM内のメモリアロケータについて説明します。

== アロケーションの流れ

G1GCにおけるオブジェクトのアロケーションの概要をVMヒープの初期化から順を追ってみていきましょう。

//image[1_heap_reserve][(1)VMヒープの予約]

まず、G1GCのVMヒープ（G1GCヒープとパーマネント領域）の最大サイズ分をメモリ領域に@<b>{予約}します（@<img>{1_heap_reserve}）。
最大G1GCヒープサイズと最大パーマネント領域サイズは言語利用者が指定できます。
未指定の場合は、G1GCヒープの最大サイズが64Mバイト、パーマネント領域の最大サイズが64Mバイト、合わせて128Mバイトがデフォルトとして設定されます（使用するOSによってデフォルト値は多少異なります）。
また、G1GCのVMヒープはリージョンのサイズでアラインメントされます。

ここまでの段階では、メモリ領域をただ予約するだけで、実際に物理メモリは割り当てられない、ということに注意してください。

//image[2_heap_commit][(2)VMヒープの確保]

次に、予約しておいたVMヒープに必要最小限のメモリ領域を@<b>{確保}します。
ここで実際に物理メモリが割り当てされます。
G1GCヒープの方はリージョン単位で確保されます（@<img>{2_heap_commit}）。

//image[3_heap_obj_alloc][(3)オブジェクトのアロケーション]

パーマネント領域のアロケーションの説明をここからは除外し、G1GCヒープ内へのアロケーションのみを見ています。
G1GCヒープにはリージョンが確保されました。
そのリージョンに対してオブジェクトがアロケーションされます（@<img>{3_heap_obj_alloc}）。

//image[4_heap_expand][(4)G1GCヒープの拡張]

オブジェクトのアロケーションによってリージョンが枯渇すると、予約しておいたメモリ領域からメモリを確保し、新たにリージョンを1個割り当て、G1GCヒープを拡張します（@<img>{4_heap_expand}）。
そして、割り当てたリージョンの中にオブジェクトをアロケーションします。

== VMヒープの予約

ここからはVMヒープの予約が実際どのように実装されているか見ていきたいと思います。

それぞれのVMヒープの初期化は@<code>{CollectedHeap}クラスを継承した子クラスの@<code>{initialize()}に記述されます。
G1GCの場合は、@<code>{G1CollectedHeap}の@<code>{initialize()}です。
VMヒープの予約はこの@<code>{initialize()}に記述されています。
VMヒープの予約処理部分だけを抜き出したものを次に示します。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
1794: jint G1CollectedHeap::initialize() {

1810:   size_t max_byte_size = collector_policy()->max_heap_byte_size();

1819:   PermanentGenerationSpec* pgs = collector_policy()->permanent_generation();

1825:   ReservedSpace heap_rs(max_byte_size + pgs->max_size(),
1826:                         HeapRegion::GrainBytes,
1827:                         UseLargePages, addr);
//}


1810行目の@<code>{collector_policy()}メンバ関数はG1GCに関するフラグや設定値等が定義されている@<code>{G1CollectorPolicy}インスタンスへのポインタを返し、@<code>{max_heap_byte_size()}メンバ関数は名前のとおり、最大G1GCヒープサイズを返します。
したがって、@<code>{max_byte_size}ローカル変数には最大G1GCヒープサイズが格納されます。

1819行目の@<code>{pgs}にはパーマネント領域に関する設定値等が定義されている@<code>{PermanentGenerationSpec}が格納されます。

1825行目で@<code>{ReservedSpace}クラスのインスタンスを生成しています。
この際に実際にVMヒープを予約しています。
@<code>{ReservedSpace}クラスのインスタンス生成には次の引数を渡します。
1827行目のほかの引数（@<code>{UseLargePages}、@<code>{addr}）については使用されませんので無視してください。

 1. 最大G1GCヒープサイズ + 最大パーマネント領域サイズ
 2. リージョンサイズ（@<code>{HeapRegion::GrainBytes}）

1. は予約するメモリ領域のサイズです。
2.はメモリ領域のアラインメントに使います。

肝心の@<code>{ReservedSpace}クラスの定義は次のとおりです。

//source[share/vm/runtime/virtualspace.hpp]{
32: class ReservedSpace VALUE_OBJ_CLASS_SPEC {
33:   friend class VMStructs;
34:  private:
35:   char*  _base;
36:   size_t _size;
38:   size_t _alignment;
//}

35行目の@<code>{_base}メンバ変数には予約したメモリ領域の先頭アドレスが格納されます。@<code>{_size}にはメモリ領域のサイズ、@<code>{_alignment}にはメモリ領域がアラインメントされた値がそれぞれ格納されます。

ここでは実装の詳細は書きませんが、今の段階ではこの@<code>{ReservedSpace}クラスを生成するとメモリ領域が予約されると考えてもらえばよいでしょう。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
1794: jint G1CollectedHeap::initialize() {

        /* 省略：ReservedSpace生成 */

1884:   ReservedSpace g1_rs   = heap_rs.first_part(max_byte_size);

1889:   ReservedSpace perm_gen_rs = heap_rs.last_part(max_byte_size);
//}

@<code>{ReservedSpace}クラスのインスタンスを生成し終えると、G1GCヒープとパーマネント領域を分割して、それぞれに対応したローカル変数（@<code>{g1_rs}・@<code>{perm_gen_rs}）に格納します。

== VMヒープの確保

予約したVMヒープ用のメモリ領域を実際に確保していくクラスが@<code>{VirtualSpace}クラスです。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.hpp]{
143: class G1CollectedHeap : public SharedHeap {

176:   VirtualSpace _g1_storage;
//}

@<code>{G1CollectedHeap}クラスには@<code>{VirtualSpace}クラスのインスタンスをもつメンバ変数が定義されています（ポインタではないことに注意してください）。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
1794: jint G1CollectedHeap::initialize() {

        /* 省略：G1GCヒープ用メモリ領域の予約 */

1891:   _g1_storage.initialize(g1_rs, 0);
//}

1891行目で@<code>{_g1_storage}メンバ変数を初期化します。
第1引数に生成したG1GCヒープ用の@<code>{ReservedSpace}のポインタを渡し、第2引数には確保するサイズを指定します。
この場合は@<code>{0}です。したがって、まだメモリ領域は確保されていません。

では、実際に確保する処理を見てみましょう。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
1794: jint G1CollectedHeap::initialize() {

1809:   size_t init_byte_size = collector_policy()->initial_heap_byte_size();

        /* 省略：G1GCヒープ用メモリ領域の予約 */

1937:   if (!expand(init_byte_size)) {
//}

@<code>{initialize()}の1809行目で@<code>{init_byte_size}に起動時に確保するメモリ領域サイズを格納します。
そして、@<code>{expand()}メンバ関数内でメモリ領域を確保します。

=== リージョンの確保

@<code>{expand()}は指定されたメモリ領域分のリージョンを確保するメソッドです。
VMヒープの初期化時、空きリージョンが枯渇したときに呼び出されます。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
1599: bool G1CollectedHeap::expand(size_t expand_bytes) {

        /* 省略:
         * expand_bytesをリージョンサイズで切り上げて
         * aligned_expand_bytes に設定
         */

1610:   HeapWord* old_end = (HeapWord*)_g1_storage.high();
1611:   bool successful = _g1_storage.expand_by(aligned_expand_bytes);
1612:   if (successful) {
1613:     HeapWord* new_end = (HeapWord*)_g1_storage.high();
1624:     expand_bytes = aligned_expand_bytes;
1625:     HeapWord* base = old_end;
1626: 
1627:     // old_endからnew_endまでのヒープリージョン作成
1628:     while (expand_bytes > 0) {
1629:       HeapWord* high = base + HeapRegion::GrainWords;
1630: 
1631:       // リージョン生成
1632:       MemRegion mr(base, high);
1634:       HeapRegion* hr = new HeapRegion(_bot_shared, mr, is_zeroed);
1635: 
1636:       // HeapRegionSeqに追加
1637:       _hrs->insert(hr);
1638:       _free_list.add_as_tail(hr);

1643:       expand_bytes -= HeapRegion::GrainBytes;
1644:       base += HeapRegion::GrainWords;
1645:     }

1667:   return successful;
1668: }
//}

前半部分で引数に受け取った@<code>{expand_bytes}をリージョンサイズで切り上げ、@<code>{aligned_expand_bytes}に設定します。

1610行目で確保中メモリ領域の終端を受け取ります。
VMヒープ初期化時にはメモリ領域はまだ確保されていませんので、予約されたVMヒープ用メモリ領域の先頭アドレスが戻ります。
同行に登場する@<code>{HeapWord*}はVMヒープ内のアドレスを指す場合に使用します。

1611行目にある@<code>{VirtualSpace}の@<code>{expand_by()}で実際のメモリ領域の確保を行います。
@<code>{expand_by()}は確保するメモリ領域のサイズを引数に取ります。
ここでは引数に@<code>{aligned_expand_bytes}、つまり確保するリージョン分のバイト数を渡しています。

メモリ領域の確保に成功した場合は、リージョンを管理する@<code>{HeapRegion}を生成します。

1629行目で@<code>{base}からリージョン1個分先のアドレスを@<code>{high}に設定します。
1632行目の@<code>{MemRegion}クラスはアドレスの範囲を管理するクラスです。
コンストラクタの引数には範囲の先頭アドレスと終端アドレスを渡します。
そして、1634行目で@<code>{HeapRegion}クラスのインスタンスを生成します。
第1引数の@<code>{_bot_shared}と第3引数の@<code>{is_zeroed}は特に関係ありませんので、ここでは無視します。

1637行目で生成した@<code>{HeapRegion}インスタンスへのポインタを@<code>{HeapRegionSeq}に追加し、1638行目で@<code>{_free_region_list}につなげばリージョン1個分のメモリ領域確保は終了です。
あとはこれを確保したメモリ領域（@<code>{aligned_expand_bytes}）の分、繰り返すだけです。

=== Windowsでのメモリ領域の予約・確保

メモリ領域の予約・確保は実際どのように実装されているのでしょうか。
実装方法はそれぞれのOSによって異なります。
まずはWindowsの実装方法を調べていきましょう。

Windowsには@<code>{VirtualAlloc()}というAPIがあります。
HotspotVMではこのAPIを使ってメモリ確保の予約、確保を実現しています。

@<code>{VirtualAlloc()}は仮想アドレス空間内のページ領域を予約または確保する、そのものズバリなAPIです。引数には次の情報を渡します。

 1. 確保、また予約したいメモリ領域の開始アドレス。NULLの場合、システムがメモリ領域の開始アドレスを決定
 2. サイズ
 3. 割り当てのタイプ
 4. アクセス保護のタイプ

では、実際にメモリ領域を予約している@<code>{os::reserve_memory()}メンバ関数を見てみましょう。

//source[os/windows/vm/os_windows.cpp]{
2717: char* os::reserve_memory(size_t bytes, char* addr, size_t alignment_hint) {
2721:   char* res = (char*)VirtualAlloc(addr, bytes, MEM_RESERVE, PAGE_READWRITE);
2724:   return res;
2725: }
//}

2721行目の第3引数に渡されている@<code>{MEM_RESERVE}がメモリ領域を予約する際のフラグです。@<code>{MEM_RESERVE}が渡されると指定されたサイズのメモリ領域は予約されるだけで実際には物理メモリに割り当てされません。

次に、メモリ領域を確保する@<code>{os::commit_memory()}メンバ関数を見てみましょう。不要な部分は省略しました。

//source[os/windows/vm/os_windows.cpp]{
2857: bool os::commit_memory(char* addr, size_t bytes, bool exec) {

2866:   bool result = VirtualAlloc(addr, bytes, MEM_COMMIT, PAGE_READWRITE) != 0;

2872:     return result;

2874: }
//}

2866行目の第3引数に渡されている@<code>{MEM_COMMIT}がメモリ領域を確保する際のフラグです。
@<code>{MEM_COMMIT}が渡されると指定されたサイズ分のメモリ領域が物理メモリとして実際に割り当てられます。

=== Linuxでのメモリ領域の予約・確保

Linuxではメモリ領域の予約・確保を@<code>{mmap()}で実装しています。

Linuxにはメモリ領域を予約するという概念はなく、@<code>{mmap()}するとメモリ領域は確保されてしまいます。ただし、メモリ領域は確保されてもすぐに物理メモリが割り当てられるわけではありません。物理メモリが割り当てられるのは確保したメモリ領域に実際にアクセスされたときです。

メモリ領域の予約にあたる部分である@<code>{os::reserve_memory()}メンバ関数のLinux版を見ていきましょう。

//source[os/linux/vm/os_linux.cpp]{
2787: char* os::reserve_memory(size_t bytes, char* requested_addr,
2788:                          size_t alignment_hint) {
2789:   return anon_mmap(requested_addr, bytes, (requested_addr != NULL));
2790: }

2751: static char* anon_mmap(char* requested_addr, size_t bytes, bool fixed) {
2752:   char * addr;
2753:   int flags;
2754: 
2755:   flags = MAP_PRIVATE | MAP_NORESERVE | MAP_ANONYMOUS;
2756:   if (fixed) {
2758:     flags |= MAP_FIXED;
2759:   }

2763:   addr = (char*)::mmap(requested_addr, bytes, PROT_READ|PROT_WRITE,
2764:                        flags, -1, 0);

2776:   return addr == MAP_FAILED ? NULL : addr;
2777: }
//}

@<code>{os::reserve_memory()}は内部で@<code>{os::anon_mmap()}を呼び出すだけです。
@<code>{os::anon_mmap()}は@<code>{MAP_ANONYMOUS}を使ってメモリ領域を確保します。
Linux版ではメモリ領域を予約するのではなく、実際は確保する点に注意してください。

2751行目で@<code>{mmap()}に渡される@<code>{flag}ローカル変数に設定している@<code>{MAP_NORESERVE}には「スワップ空間の予約をおこなわない」という意味があります。@<code>{mmap()}してアドレスが確保された場合、そのメモリ領域に確実に割り当てられる保証を得るために、スワップ空間をサイズ分一気に予約してしまうOSがあります。
@<code>{MAP_NORESERVE}にはそれを防ぐ効果があります。@<code>{os::reserve_memory()}の段階ではVMヒープ用のメモリ領域を予約するだけで、実際にオブジェクトをアロケーションするなどのアクセスを行いません。
そのため、スワップ空間を予約するのは無駄だということです。HP-UX@<fn>{hp-ux}のようにスワップ空間を予約するOSには効果のある工夫です。

メモリ領域の確保にあたる部分である@<code>{os::commit_memory()}メンバ関数では、逆に確保したいアドレス分だけ@<code>{MAP_NORESERVE}を付けずに@<code>{mmap()}します。
似たような処理のため、コードの紹介はしません。

Linuxの場合、実際に物理メモリに割り当てられるタイミングはWindowsとは異なります。
確保したメモリ領域にオブジェクトがアロケーションされ、実際にアクセスされたときに物理メモリが割り当てられます。

//footnote[hp-ux][HP-UX：ヒューレット・パッカード社（HP 社）製の UNIX オペレーティングシステム]

=== VMヒープのアラインメント実現方法

VMヒープはリージョンのサイズでアラインメントされています。
つまり、VMヒープの先頭アドレスはリージョンサイズの倍数になっているということです。
HotspotVMではこのアラインメントをどのようにして実現しているのでしょうか？

実装方法は拍子抜けするほどシンプルです。
具体的には次の手順で処理を行います。
ここでは説明を簡単にするため、アラインメントサイズ（リージョンサイズ）を1Kバイト、VMヒープのサイズは1Kバイトよりも大きいと仮定します。

  1. VMヒープサイズ分のメモリ領域を予約
  2. 1.で予約したメモリ領域の範囲に1Kバイトの倍数アドレスを記録
  3. 予約したメモリ領域を破棄
  4. 2.で記録しておいたアドレスを指定し、再度VMヒープサイズ分のメモリ領域を予約
  5. 4.に失敗した場合は1.に戻る

//image[vm_heap_align][アラインメントされたVMヒープの予約]

まず、1.でVMヒープサイズ分のメモリ領域を予約してしまいます。
メモリ領域の予約には@<code>{os::reserve_memory()}関数を使います。

予約したメモリ領域の範囲内には1Kバイトの倍数アドレスがどこかにはあるはずです。
1Kバイト以上のメモリ領域を予約しているのですから当然ですね。
その1Kバイトの倍数アドレスが、アラインメントされたアドレスになります。
ここで重要なのは上記で取得したアラインメントされたアドレスが、OSが「このメモリ領域は使えるよ」と返してきたものだということです。
2.ではそのアドレスを記録しておきます。

3.では1.で予約したメモリ領域を一度破棄してしまいます。
メモリ領域は予約しているだけであり、実メモリには割り当てられてはいませんので、破棄にかかるコストは微々たるものです。

4.では2.で保持しておいたアドレスを指定して、VMヒープサイズ分のメモリ領域を予約します。
メモリ領域の予約を実際におこなう@<code>{mmap()}や@<code>{VirtualAlloc()}は予約するメモリ領域の先頭アドレスを指定することができますので、これを利用します。

ただし、2.の段階では使ってよかったはずのアドレスが、4.の段階で使えなくなっていることも考えられます。
そのため、失敗した場合は1.に戻り成功するまで処理を繰り返します。

== オブジェクトのアロケーション

次はVMヒープ上の確保されたリージョンからオブジェクトをアロケーションする部分を見ていきましょう。

=== アロケーションの流れ

@<code>{CollectedHeap}の共通のインタフェースから、実際のG1GCのVMヒープからオブジェクトが割り当てられるまでのシーケンス図を@<img>{sequence_alloc}に示します。

//image[sequence_alloc][オブジェクトアロケーションの流れ]

まず、VMはオブジェクトの割り当て要求として@<code>{CollectedHeap::obj_allocate()}を呼び出します。
次に、@<code>{CollectedHeap}は@<code>{Universe::heap()}を呼び出して、起動オプションで選択されたVMヒープクラス（この場合は@<code>{G1CollectedHeap}）のインスタンスを取得します。
そして、VMヒープクラス共通の@<code>{mem_allocate()}を呼び出し、必要なサイズのメモリ領域の割り当てを行います。
@<code>{G1CollectedHeap}内部でVMヒープから適切にメモリを切り出し、最終的に@<code>{CollectedHeap}に割り当てたメモリ領域を返します。
その後、指定されたオブジェクト種類に応じたセットアップを行い、VMに返却します。

=== G1GCのVMヒープへメモリ割り当て

@<code>{G1CollectedHeap}内でおこわれる、VMヒープへのメモリ割り当てを見ていきましょう。
まずは、@<code>{G1CollectedHeap}の@<code>{mem_allocate()}です。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
830: HeapWord*
831: G1CollectedHeap::mem_allocate(size_t word_size,
832:                               bool   is_noref,
833:                               bool   is_tlab,
834:                               bool*  gc_overhead_limit_was_exceeded) {

843:     HeapWord* result = NULL;

845:       result = attempt_allocation(word_size, &gc_count_before);

849:     if (result != NULL) {
850:       return result;
851:     }

         /* 省略: GC実行 */

884: }
//}

845行目でオブジェクトのサイズを指定して@<code>{attempt_allocation()}を呼び出します。
もし割り当てられなければGCを実行してVMヒープに空きを作るのですが、ここでは省略します。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.inline.hpp]{
63: inline HeapWord*
64: G1CollectedHeap::attempt_allocation(size_t word_size,
65:                                     unsigned int* gc_count_before_ret) {

70:   HeapWord* result = _mutator_alloc_region.attempt_allocation(word_size,
71:                                                       false /* bot_updates */);
72:   if (result == NULL) {
73:     result = attempt_allocation_slow(word_size, gc_count_before_ret);
74:   }

79:   return result;
80: }
//}

70行目の@<code>{attempt_allocation()}は@<code>{G1AllocRegion}のメンバ関数で、リージョンからオブジェクトを割り当てようと試みます。
@<code>{G1AllocRegion}はオブジェクトを割り当てるリージョンを管理するクラスで、空きのあるリージョンを内部で1つだけ保持しています。

もし@<code>{G1AllocRegion}内のリージョンに必要な分の空きがなく、割り当てに失敗したら、73行目で@<code>{attempt_allocation_slow()}が呼び出されます。
@<code>{attempt_allocation_slow()}では@<code>{G1AllocRegion}に対して、新たな空きリージョンが設定され、リージョンに対して必要なサイズ分が割り当てられます。
処理の内容については本筋とはそれほど関係ありませんので割愛します。

//source[share/vm/gc_implementation/g1/g1AllocRegion.inline.hpp]{
55: inline HeapWord* G1AllocRegion::attempt_allocation(size_t word_size,
56:                                                    bool bot_updates) {

59:   HeapRegion* alloc_region = _alloc_region;

62:   HeapWord* result = par_allocate(alloc_region, word_size, bot_updates);
63:   if (result != NULL) {

65:     return result;
66:   }

68:   return NULL;
69: }
//}

59行目に登場する@<code>{_alloc_region}メンバ変数が、@<code>{G1AllocRegion}が管理する空きリージョンです。
62行目でその空きリージョンを引数に@<code>{par_allocate()}を呼び出します。

@<code>{par_allocate()}からいくつかの関数を経由し、最終的に空きリージョン（@<code>{HeapRegion}）の@<code>{allocate_impl()}メンバ関数を呼び出します。

@<code>{allocate_impl()}は@<code>{HeapRegion}が継承している@<code>{ContiguousSpace}クラスに定義してあるメンバ関数で、実際にリージョンからメモリ領域を確保する役割を持ちます。

//source[share/vm/memory/space.cpp]{
827: inline HeapWord* ContiguousSpace::allocate_impl(size_t size,
828:                                                 HeapWord* const end_value) {
838:   HeapWord* obj = top();
839:   if (pointer_delta(end_value, obj) >= size) {
840:     HeapWord* new_top = obj + size;
841:     set_top(new_top);
843:     return obj;
844:   } else {
845:     return NULL;
846:   }
847: }
//}

@<code>{allocate_impl()}の引数である@<code>{size}にはオブジェクトサイズのバイト数ではなく、ワード数が渡されます。
もう1つの引数、@<code>{end_value}にはリージョン内チャンクの終端アドレスが渡されます。

839行目の@<code>{pointer_delta()}は指定されたアドレスの差分をワード数で戻す関数です。
ここでは、リージョン内の空き領域の先頭を指す@<code>{_top}と、@<code>{end_value}の差分を求め、空きメモリ領域のサイズをワード数で返します。
もし、@<code>{size}分の空きがなければ845行目で@<code>{NULL}を戻します。

リージョンに充分な空きがあれば、840行目で@<code>{obj}を@<code>{size}分ずらして、841行目でチャンクの先頭アドレスに設定します。
そして、確保したメモリ領域の先頭（@<code>{obj}）を843行目で戻します。

== TLAB（Thread Local Allocation Buffer）

アロケーションの工夫の1つとして@<b>{TLAB}について簡単に説明しておきましょう。

VMヒープはすべてのスレッドの共有領域です。
そのため、VMヒープからオブジェクトをアロケーションする際にはVMヒープ全体をロックし、ほかのスレッドからのアロケーションが割り込まないようにする必要があります。

しかし、せっかく別々のCPUコアで動作していたスレッドをアロケーション時にいちいちロックしてしまうのは嬉しくありません。
その問題を解決するためにそれぞれのスレッド専用のアロケーション用バッファを持たせてロックの回数を少なくしよう、というのがTLABの考えです。

あるスレッドの最初のオブジェクトアロケーション時に、一定サイズのメモリ領域をVMヒープからアロケーションし、スレッド内にバッファとして貯め込みます。
このバッファをTLABと呼びます。
VMヒープのロックが必要なのは、TLABを確保するときのみです。

同スレッドからの次のオブジェクトアロケーション時には、TLAB内からオブジェクトサイズ分アロケーションします。
この時は他スレッドからアクセスされる可能性がないため、VMヒープのロックは必要ありません。

//image[tlab][TLABによるアロケーション]

TLABは通常はオフになっていますが、言語利用者がJavaの起動オプションによってオンにできます。
さらにTLABのサイズも指定可能です。

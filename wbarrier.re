= ライトバリアのコスト
いよいよ本書も最後の章となりました。
最後の章ではライトバリアのコストについて取り上げます。

== 実行時のGC切り替え
本書でも説明してきたとおり、HotspotVMは複数のGCアルゴリズムを選択することができます。
しかも、起動オプションとしてGCアルゴリズムを指定しますので、Javaプログラムを実行するときにGCを切り替えなくてはいけません。

=== 性能劣化の懸念
実行時にGCを切り替える以外にも、たとえばコンパイル時にGCを切り替えるという方法があります。
G1GC用にコンパイルしたOpenJDK、CMS用にコンパイルしたOpenJDK、という風に異なるGCアルゴリズムごとにビルドしたバイナリを配布すればよいでしょう。

ですが、この方法だと開発者はGCアルゴリズムを追加するたびにバイナリを増やさないといけません。
また管理するバイナリが増えてしまい、いらない手間がかかってしまうことは容易に想像が付きます。
また、ユーザの利便性という面からあまり現実的ではありません。
やはり、実行時にGCを切り替えつつ、お手軽にいろいろ試したいわけですから。

実行時にGCを切り替える方法はいいことずくめに見えますが、コンパイル時にGCを切り替える方法と比べて性能劣化をまねくという欠点があります。
具体的なサンプルコードとしてC言語で書いたGCを実行する@<code>{gc_start()}という関数を以下に示します。

//list[dynamic_gc_options_with_c][例: 実行時GC切り替えのGC起動関数]{
void
gc_start(gc_state state) {
  switch (state) {
    case gc_state_g1gc;
      g1gc_gc_start();
      break;
    case gc_state_cms;
      cms_gc_start();
      break;
    case gc_state_serial;
      serial_gc_start();
      break;
  }
};
//}

//list[static_gc_options_with_c][例: コンパイル時GC切り替えのGC起動関数]{
void
gc_start(void) {
#ifdef GC_STATE_G1GC
  g1gc_gc_start();
#elif GC_STATE_CMS
  cms_gc_start();
#elif GC_STATE_SERIAL
  serial_gc_start();
#endif
};
//}

@<list>{dynamic_gc_options_with_c}では@<code>{gc_start()}を実行するときに条件分岐の処理が入ってしまいます。
一方、@<list>{static_gc_options_with_c}はコンパイル時に@<code>{gc_start()}内で呼び出す関数が決定するので、実行時に条件分岐の処理が不要です。

=== ライトバリアのコスト増加
実行時にGCを切り替える際、もっとも性能劣化が懸念される場所はライトバリアです。
ライトバリアは頻繁に実行され、ボトルネックになりやすい処理です。
異なるライトバリアが必要なGCを実行時に切り替える場合には、実行時にライトバリアを切り替える必要があります。
つまり、@<list>{dynamic_gc_options_with_c}で説明した条件分岐のような切り替えが必要になってくるわけです。
そのため、ライトバリアに余計にコストがかかってしまい、ミューテータの速度に影響がでてしまいます。

== インタプリタのライトバリア
では、どのようにしてGCごとにライトバリアを切り替えているか、実際に実装を見ていきましょう。
まずは、JITコンパイラを利用していない、素直にJavaバイトコードを実行するインタプリタで実行されるライトバリアについてです。

=== ライトバリアの切り替え
@<code>{oop_store()}という関数によって、オブジェクトフィールドへ参照型の値を格納します。

//source[share/vm/oops/oop.inline.hpp]{
518: template <class T> inline void oop_store(volatile T* p, oop v) {
519:   update_barrier_set_pre((T*)p, v);
521:   oopDesc::release_encode_store_heap_oop(p, v);
522:   update_barrier_set((void*)p, v);
523: }
//}

521行目でフィールド内に値を格納します。
519行目の@<code>{update_barrier_set_pre()}関数がフィールドに値を設定する前のライトバリアで、522行目の@<code>{update_barrier_set()}が設定した後のライトバリアです。

//source[share/vm/oops/oop.inline.hpp]{
499: inline void update_barrier_set(void* p, oop v) {
501:   oopDesc::bs()->write_ref_field(p, v);
502: }
503: 
504: template <class T> inline void update_barrier_set_pre(T* p, oop v) {
505:   oopDesc::bs()->write_ref_field_pre(p, v);
506: }
//}

上記に示した通り、それぞれの関数は@<code>{oopDesc::bs()}で取得したインスタンスに対して、関数を呼び出しているだけです。
この@<code>{oopDesc::bs()}は@<code>{SharedHeap}クラスの@<code>{set_barrier_set()}というメンバ関数で設定されます。

//source[share/vm/memory/sharedHeap.cpp]{
273: void SharedHeap::set_barrier_set(BarrierSet* bs) {
274:   _barrier_set = bs;
276:   oopDesc::set_bs(bs);
277: }
//}

そして、@<code>{set_barrier_set()}はそれぞれのVMヒープクラスの初期化時に呼び出されます。
G1GCの場合には@<code>{G1SATBCardTableLoggingModRefBS}というクラスのインスタンスが、それ以外の場合は@<code>{CardTableModRefBSForCTRS}というクラスのインスタンスが@<code>{set_barrier_set()}の引数として渡されます。
@<code>{G1SATBCardTableLoggingModRefBS}と@<code>{CardTableModRefBSForCTRS}はどちらとも@<code>{BarrierSet}の子クラスとして定義されたクラスです。

では、@<code>{BarrierSet}の@<code>{write_ref_field_pre()}と@<code>{write_ref_field()}の中身を見てみましょう。

//source[share/vm/memory/barrierSet.inline.hpp]{
35: template <class T> void BarrierSet::write_ref_field_pre(
                              T* field, oop new_val) {
36:   if (kind() == CardTableModRef) {
37:     ((CardTableModRefBS*)this)->inline_write_ref_field_pre(field, new_val);
38:   } else {
39:     write_ref_field_pre_work(field, new_val);
40:   }
41: }
42: 
43: void BarrierSet::write_ref_field(void* field, oop new_val) {
44:   if (kind() == CardTableModRef) {
45:     ((CardTableModRefBS*)this)->inline_write_ref_field(field, new_val);
46:   } else {
47:     write_ref_field_work(field, new_val);
48:   }
49: }
//}

それぞれのメンバ関数で分岐しているのがわかります。
36・44行目で@<code>{kind()}が@<code>{CardTableModRef}だった場合、自身は@<code>{CardTableModRefBSForCTRS}のインスタンスと判断し、適切な関数を呼び出します。
それ以外は@<code>{write_ref_field(_pre)_work()}を呼び出します。

//source[share/vm/memory/barrierSet.hpp]{
 99:   virtual void write_ref_field_pre_work(      oop* field, oop new_val) {};
// ..
106:   virtual void write_ref_field_work(void* field, oop new_val) = 0;
//}

それぞれ@<code>{BarrierSet}クラスの仮想関数として定義してありますが、今のところ実装しているのは@<code>{G1SATBCardTableLoggingModRefBS}クラスだけです。
つまり、現在のHotspotVMのライトバリアはG1GCとそれ以外のものの2種類を実行時に切り替えて動作しているのです。

=== G1GCが入るまでライトバリアの種類はひとつだけだった
調べて驚いたのですけど、G1GCが入る前（OpenJDK7より前）はライトバリアの実行時切り替えがないんですね。
カードテーブルに書き換えられたことを記録するだけの単純なものだけしか実装されていませんでした（@<code>{CardTableModRefBSForCTRS}のみ）。
よく考えてみたら世代別もインクリメンタルGCもそれだけでいけるんですよね…。
G1GCのライトバリアが特殊すぎるだけだよな、と。

OpenJDK7からはG1GCの導入によってライトバリアの切り替えが発生しますので、インタプリタのオブジェクトへの代入操作は少しだけ性能が劣化するでしょう。

== JITコンパイラのライトバリア
HotspotVMではある程度の呼び出し回数を超えたメソッドはJITコンパイルするという特徴があります。
もしメソッド内でオブジェクトフィールドへの代入があれば、ライトバリアの処理も一緒にマシン語にコンパイルされます。

今まで実行時のライトバリアの切り替えは条件分岐が入ってしまいコストがかかるという話をしてきましたが、JITコンパイラが絡んでくるとこの状況は変わってきます。

=== C1コンパイラ
JITコンパイラには@<b>{C1}、@<b>{C2}、@<b>{Shark}とよばれる3種類のコンパイラがあります。
本書ではそのうちC1を取り上げたいと思います。

C1はクライアントサイドで利用するシーンで使われるJITコンパイラで、Javaの起動オプションで@<code>{-client}を指定したときに利用されます。
クライアント側で利用するためコンパイル時間が比較的短く、メモリ使用量も少ない代わりにまぁまぁの最適化をおこなうという特徴があります。

=== ライトバリアのマシン語生成
オブジェクトフィールドへの代入操作をJITコンパイルしている箇所は@<code>{LIRGenerator}クラスの@<code>{do_StoreField()}というメンバ関数です。

//source[share/vm/c1/c1_LIRGenerator.cpp]{
1638: void LIRGenerator::do_StoreField(StoreField* x) {

1708:   if (is_oop) {
1710:     pre_barrier(LIR_OprFact::address(address),
1711:                 LIR_OprFact::illegalOpr /* pre_val */,
1712:                 true /* do_load*/,
1713:                 needs_patching,
1714:                 (info ? new CodeEmitInfo(info) : NULL));
1715:   }
1716: 
1717:   if (is_volatile && !needs_patching) {
1718:     volatile_field_store(value.result(), address, info);
1719:   } else {
1720:     LIR_PatchCode patch_code =
            needs_patching ? lir_patch_normal : lir_patch_none;
1721:     __ store(value.result(), address, info, patch_code);
1722:   }
1723: 
1724:   if (is_oop) {
1726:     post_barrier(object.result(), value.result());
1727:   }

//}

1717〜1722行目がオブジェクトフィールドへの代入操作のマシン語を生成している部分です。
ライトバリアの生成は@<code>{pre_barrier()}と@<code>{post_barrier()}の中でおこなわれます。

まずは@<code>{pre_barrier()}を見てみましょう。

//source[share/vm/c1/c1_LIRGenerator.cpp]{
1386: void LIRGenerator::pre_barrier(
             LIR_Opr addr_opr, LIR_Opr pre_val,
1387:        bool do_load, bool patch, CodeEmitInfo* info) {
1389:   switch (_bs->kind()) {

1391:     case BarrierSet::G1SATBCT:
1392:     case BarrierSet::G1SATBCTLogging:
1393:       G1SATBCardTableModRef_pre_barrier(
              addr_opr, pre_val, do_load, patch, info);
1394:       break;

1396:     case BarrierSet::CardTableModRef:
1397:     case BarrierSet::CardTableExtension:
1398:       // No pre barriers
1399:       break;
1400:     case BarrierSet::ModRef:
1401:     case BarrierSet::Other:
1402:       // No pre barriers
1403:       break;
1404:     default      :
1405:       ShouldNotReachHere();
1406: 
1407:   }
1408: }
//}

1389行目に登場している@<code>{kind()}というのはインタプリタのライトバリアで説明したのと同じものです。
もしG1GCのものであれば、1393行目のcase文に入りG1GC用のライトバリアをおこなうマシン語を生成します。
それ以外であれば何も生成しません。
1400〜1403行目のcaseは通らない場所なので単純に無視してください。


次に@<code>{post_barrier()}を見てみます。

//source[share/vm/c1/c1_LIRGenerator.cpp]{
1410: void LIRGenerator::post_barrier(
             LIR_OprDesc* addr, LIR_OprDesc* new_val) {
1411:   switch (_bs->kind()) {

1413:     case BarrierSet::G1SATBCT:
1414:     case BarrierSet::G1SATBCTLogging:
1415:       G1SATBCardTableModRef_post_barrier(addr,  new_val);
1416:       break;

1418:     case BarrierSet::CardTableModRef:
1419:     case BarrierSet::CardTableExtension:
1420:       CardTableModRef_post_barrier(addr,  new_val);
1421:       break;
1422:     case BarrierSet::ModRef:
1423:     case BarrierSet::Other:
1424:       // No post barriers
1425:       break;
1426:     default      :
1427:       ShouldNotReachHere();
1428:     }
1429: }
//}

こちらも同じように@<code>{kind()}の値をみてどのライトバリアを生成するか決定しています。
G1GCであれば1415行目でG1GC用のライトバリアを生成します。
それ以外であれば1420行目でカードテーブルに単純に書き換えを記録するライトバリアを生成します。
1422〜1424行目のcaseは通らない場所なので単純に無視します。

このようにJITコンパイラの時点ではすでに利用するGCアルゴリズムは決定しているので、そのGCにあったライトバリアを生成することができます。
そのため、JITコンパイルされたコードではライトバリア切り替えのコストが0で済むのです。

JITさん、カワイイ…。

== おわりに
もう終わりですし、きっとこんなところはみんな読み飛ばしそうなのでグチをこぼしておきたいとおもいます。

題して「OpenJDKのソースコードのどこが読みづらいか」。

端的に言えば抽象化をやりすぎているところが読みづらいです。

たとえばコールバック地獄。
関数の引数にとったインスタンスの関数を呼び出して、さらにその呼び出した関数の引数にとったインスタンスの関数を呼び出して、さらに…みたいなコードが平気であるわけですよ。

（コールバックの使いすぎには注意って小一時間説教したい！）

あと継承地獄。
継承関係が4階層・5階層と平気であります。これはさすがにコード読んでいて迷います。
またクラス分けの粒度が歴史的な背景もあってツギハギになっている箇所がちらほらあり、とてもじゃないですが一貫性があるとは思えない。
一貫性があればまだ覚えられるのですが…。

（一度ソースコードを窓から捨てることをお勧めしたくなる！）

とはいえ、やはり現在進行形で拡張され続けているソースコードだけあって、抽象化は大変うまくできています。
VMとOS間の抽象化も実用的にできているし、GCも容易に追加できるようになっているし。
慣れ親しんだ開発者にとってはとてもハックしやすい、まさに「おれたちのVM」感があって憎めないな、と感じました。

ブツブツ文句をいいながらも楽しみながら読めたのはやはりOpenJDKを実装してきてくれた開発者のみなさんのおかげです。
こんなに楽しく読めるものをほんとうにありがとうございます（皮肉じゃないよ）。
拡張して成長していくさまを、また眺めにきます！

おしまい！

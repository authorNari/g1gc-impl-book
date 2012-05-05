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

=== G1GCが入るまでライトバリアは切り替えてなかった
調べて驚いたのですけど、G1GCが入る前（OpenJDK6以降）はライトバリアの実行時切り替えがないんですね。
カードテーブルに書き換えられたことを記録するだけの単純なものだけしか実装されていませんでした（@<code>{CardTableModRefBSForCTRS}のみ）。
よく考えてみたら世代別もインクリメンタルGCもそれだけでいけるんですよね…。
G1GCのライトバリアが特殊すぎるだけだよな、と。

OpenJDK7からはG1GCのおかげでライトバリアの切り替えが発生しますので、インタプリタのオブジェクトへの代入操作は少しだけ性能が劣化するでしょう。

== JITコンパイラのライトバリア

=== C1コンパイラ

=== ライトバリアのマシン語生成

== おわりに

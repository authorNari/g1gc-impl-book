= 準備
HotspotVMとは何かという点から話をはじめましょう。

== HotspotVMとは

HotspotVMはOracle社主導で開発されているもっともポピュラーなJavaVMです。

HotspotVMの特徴は「プログラムの実行頻度の高い部分のみ機械語にコンパイルする」という点です。
これにはプログラムの実行時間を多く費やす部分（実行頻度の高い部分）を最適化し、プログラム全体の実行時間を短くしようという狙いがあります。
また、機械語へのコンパイルをある程度の範囲に絞るため、コンパイル時間が短くなるという効果もあります。
「実行頻度の高い部分」を@<b>{Hotspot}と呼びます。この点がHotspotVMの名前の由来となっています。

HotspotVMのもう1つの特徴は複数のGCアルゴリズムが実装されているという点でしょう。
通常GCアルゴリズムは、レスポンス性能とスループット性能のどちらかを優先してチューンアップします。一般的にレスポンス性能を優先したGCアルゴリズムはスループット性能が低下します。
逆にスループット性能を優先したGCアルゴリズムはレスポンス性能が低下します。
そのた、さまざまな要素から現在のところは完璧なGCというものが存在していません。
HotspotVMはこのジレンマに対する回答として@<b>{複数のGCアルゴリズムを実装する}という手法をとりました。
プログラマはアプリケーションの特性に合わせてHotspotVMのGCアルゴリズムを選択できます。
つまり、レスポンス性能を求めるアプリケーションの場合は、それに適したGCアルゴリズムを「プログラマ」が選択できるということです。プログラマによるGCアルゴリズムの選択は優れた手法だと言えるでしょう。

== OpenJDKとは

Javaの開発用プログラミングツール群のことをまとめて「Java SE Development Kit（JDK）」と呼びます。

JDKにはHotspotVMの他に、JavaのソースコードをJavaバイトコードにコンパイルするJavaコンパイラ、Javaソースコードからドキュメントを生成するツールなどが同梱されています。

2006年11月、当時のSunはJDKのソースコードをGPLv2@<fn>{gpl}の下で無償公開することを発表しました。
このオープンソース版のJDKを@<b>{OpenJDK}と呼びます。

OpenJDKの最新バージョンは@<b>{OpenJDK7}と呼ばれています。
一方、オラクルが正式に提供するJDKの最新バージョンは@<b>{JDK7}と呼ばれています。
OpenJDK7とJDK7は名前は異なるものですが、両者のコードはほぼ同じです。
ただし、JDKにはライセンス的にクローズドなコードが一部あるため、OpenJDKではそれをオープンソースとして書き直しているようです。

//footnote[gpl][GPLv2（GNU General Public License）：コピーレフトのソフトウェアライセンスの第2版]

== ソースコード入手

OpenJDKの公式サイトは次のURLです。

@<href>{http://openjdk.java.net/}

//image[openjdk_site][OpenJDK公式サイト]

現在の最新のリリース版はOpenJDK7です。開発マイルストーンは次のURLで現在でも閲覧可能です。

@<href>{http://openjdk.java.net/projects/jdk7/milestones/}

本章ではOpenJDK7の執筆時の最新バージョンである「jdk7-b147」を解説対象とします。

では、ソースコードをダウンロードしましょう。

OpenJDK7の最新開発バージョンのソースコードは次のURLからダウンロードできます。

@<href>{http://download.java.net/openjdk/jdk7/}

特定の開発バージョンのソースコードを入手したい場合は「Mercurial」@<fn>{mercurial}を使います。
MercurialとはPythonで作られたフリーの分散バージョン管理システムです。

OpenJDKは複数のプロジェクトを持っており、プロジェクト分のリポジトリが存在します。
今回はMercurialを使ってHotspotVMプロジェクトのリポジトリのみからソースコードを入手しましょう。
次のコマンドを入力すると、リポジトリからコードツリーをチェックアウトできます。

//emlist{
hg clone -r jdk7-b147 http://hg.openjdk.java.net/jdk7/jdk7/hotspot hotspot
//}

//footnote[mercurial][Mercurial公式サイト：@<href>{http://mercurial.selenic.com/wiki/}]

== ソース構成

HotspotVMのソースコード内の「@<code>{src}」という名前のディレクトリにHotspotVMのソースコードが置かれています。

//table[dir][ディレクトリ構成]{
ディレクトリ	概要
------------------------------
@<code>{cpu}		CPU依存コード群
@<code>{os}		OS依存コード群
@<code>{os_cpu}		OS、CPU依存コード群（Linuxでかつx86など）
@<code>{share}		共通コード群
//}

@<table>{dir}内最後の「@<code>{share}」ディレクトリ内には「@<code>{vm}」というディレクトリがあります。
この「@<code>{vm}」ディレクトリの中にHotspotVMの大半部分のコードが置かれています。

//table[vmdir][vm内ディレクトリ構成]{
ディレクトリ	概要
------------------------------
@<code>{c1}			C1コンパイラ
@<code>{classfile}		Javaクラスファイル定義
@<code>{gc_implementation}	GCの実装部
@<code>{gc_interface}		GCのインターフェイス部
@<code>{interpreter}		Javaインタプリタ
@<code>{oops}			オブジェクト構造定義
@<code>{runtime}		VM実行時に必要なライブラリ
//}

また、「@<code>{src}」ディレクトリ内のソースコード分布を@<table>{source_stat}に示します。

//table[source_stat][ソースコード分布]{
言語	ソースコード行数	割合
---------------------------
C++		420,791 		93%
Java	21,231			5%
C		7,432			2%
//}

HotspotVMは約45万行のソースコードから成り立っており、そのほとんどがC++で書かれています。

== 特殊なクラス

HotspotVM内のほとんどのクラスは次の2つのクラスのいずれかを継承しています。

 * @<code>{CHeapObj}クラス
 * @<code>{AllStatic}クラス

ソースコード上ではこれらのクラスが頻出しますので、ここで意味をおさえておきましょう。

=== CHeapObjクラス

@<code>{CHeapObj}クラスはCのヒープ領域上のメモリで管理されるクラスです。
@<code>{CHeapObj}クラスを継承するクラスのインスタンスは、Cヒープ上にメモリ確保されます。

@<code>{CHeapObj}クラスの特殊な点はオペレータの@<code>{new}と@<code>{delete}を書き換え、C++の通常のアロケーションにデバッグ処理を追加してるところです。
このデバッグ処理は開発時にのみ使用されます。

@<code>{CHeapObj}クラスにはいくつかのデバッグ処理が実装されていますが、今回はその中の@<b>{メモリ破壊検知機能}を見てみましょう。

デバッグ時、@<code>{CHeapObj}クラス（または継承先クラス）のインスタンスを生成する際に、わざと余分にアロケーションします。
アロケーションされたメモリのイメージを@<img>{cheap_obj_debug_by_new}に示します。

//image[cheap_obj_debug_by_new][デバッグ時のCHeapObjインスタンス]

余分にアロケーションしたメモリ領域を@<img>{cheap_obj_debug_by_new}に示すとおり「メモリ破壊検知領域」として使います。
メモリ破壊検知領域には@<code>{0xAB}という値を書き込んでおきます。
@<code>{CHeapObj}クラスのインスタンスとして@<img>{cheap_obj_debug_by_new}の真ん中に示したメモリ領域を使用します。

そして、@<code>{CHeapObj}クラスのインスタンスの@<code>{delete}時にメモリ破壊検知領域が@<code>{0xAB}のままであるかをチェックします。
もし、書き換わっていれば、@<code>{CHeapObj}クラスのインスタンスの範囲を超えたメモリ領域に書き込みがおこなわれたということです。
これはメモリ破壊がおこなわれた証拠となりますので、発見次第エラーを出力し、終了します。

=== AllStaticクラス

@<code>{AllStatic}クラスは「静的な情報のみをもつクラス」という意味を持つ特殊なクラスです。

@<code>{AllStatic}クラスを継承したクラスはインスタンスを生成なくなります。
そのため、グローバル変数や関数を1つの名前空間にまとめたいときに、@<code>{AllStatic}クラスを継承します。
継承するクラスにはグローバル変数やそのアクセサ、静的（static）なメンバ関数など、クラスから使える情報のみが定義されます。

== 各OS用のインタフェース

HotspotVMはさまざまなOS上で動作する必要があります。
そのため、各OSのAPIを統一のインタフェースを使って扱う便利な機構が用意されています。

//source[share/vm/runtime/os.hpp]{
80: class os: AllStatic {
       ...
223:   static char*  reserve_memory(size_t bytes, char* addr = 0,
224:                                size_t alignment_hint = 0);
       ...
732: };
//}

@<code>{os}クラスは@<code>{AllStatic}クラスを継承するためインスタンスを作らずに利用します。

@<code>{os}クラスに定義されたメンバ関数の実体は各OSに対して用意されています。

 * os/posix/vm/os_posix.cpp
 * os/linux/vm/os_linux.cpp
 * os/windows/vm/os_windows.cpp
 * os/solaris/vm/os_solaris.cpp

上記ファイルはOpenJDKのビルド時に各OSに合う適切なものが選択され、コンパイル・リンクされます。
@<code>{os/posix/vm/os_posix.cpp}はPOSIX API準拠のOS（LinuxとSorarisの両方）に対してリンクされます。例えばLinux環境では@<code>{os/posix/vm/os_posix.cpp}と@<code>{os/linux/vm/os_linux.cpp}がリンクされます。

そのため、例えば上記の@<code>{share/vm/runtime/os.hpp}で定義されている@<code>{os::reserve_memory()}を呼び出し時には、各OSで別々の@<code>{os::reserve_memory}が実行されます。

@<code>{os:xxx()}というメンバ関数はソースコード上によく登場しますので、よく覚えておいてください。


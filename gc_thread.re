= GCスレッド

『アルゴリズム編』で述べたとおり、G1GCは並列・並行GCを組み合わせたGCです。
本章ではHotspotVMが実装しているスレッドの概要と、GCによるスレッドの利用方法を解説します。

== TODO Threadクラス

HotspotVM内ではスレッドを扱うための基本的な機能を@<code>{Thread}クラスに実装し、@<code>{Thread}クラスを継承した子クラスによって実際にスレッドを生成・管理します。
@<img>{thread_hierarchy}に@<code>{Thread}クラスの継承関係を示します。

//image[thread_hierarchy][Threadクラスの継承関係]

@<code>{Thread}クラスは@<code>{CHeapObj}クラスを直接継承しているため、Cのヒープ領域から直接アロケーションされます。

親クラスの@<code>{ThreadShadow}クラスはスレッド実行中に発生した例外を統一的に扱うためのクラスです。

子クラスの@<code>{JavaThread}クラスはJavaの言語レベルで実行されるスレッドを表現しています。言語利用者がJava上でスレッドを一つ作ると、内部では@<code>{JavaThread}クラスが一つ生成されています。

子クラスの@<code>{NamedThread}クラスはVMの内部だけで利用する一意な名前付きのスレッドを表現します。この@<code>{NamedThread}クラスを継承してGCスレッドが実装されています。

=== スレッド生成

=== スレッド実行

=== スレッド停止

== TODO 排他制御
* Park
* Monitor
* Mutex

== TODO GC並列スレッド

== TODO GC並行スレッド

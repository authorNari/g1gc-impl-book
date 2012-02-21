= VMヒープの抽象化

HotspotVMではさまざまなGCアルゴリズムを選択できる機能があります。
Javaの起動オプションで「-XX:UseParallelGC」などと指定するアレのことです。
GCはそれぞれ管理するヒープの形状が異なり、また当然ですがGCアルゴリズム自体も異なります。
本章ではHotspotVMのヒープやGCの抽象化がどのように設計されているかを見ていきます。

== VMヒープクラス

HotspotVMにはVMヒープを表現するクラスが定義されています。

//image[collected_heap_hierarchy][CollectedHeapクラスの継承関係]

@<img>{collected_heap_hierarchy}に示す通り、VMヒープは@<code>{CollectedHeap}という抽象的なクラスで統一的に扱われます。
@<code>{CollectedHeap}クラスはGCアルゴリズムによって子クラスに派生し、この子クラスがVMヒープの実体となります。

== GCを指定するOpenJDK7の起動オプション

@<table>{java_options}にOpenJDK7のGCを指定する起動オプションと利用されるVMヒープクラスの対応表を示します。

//table[java_options][起動オプションと利用するVMヒープクラス]{
オプション		GCアルゴリズム		VMヒープクラス
---------------------------------------------------------------
-XX:UseSerialGC		逐次GC		@<code>{GenCollectedHeap}
-XX:UseParallelGC	並列GC		@<code>{ParallelScavengeHeap}
-Xincgc			インクリメンタルGC	@<code>{GenCollectedHeap}
-XX:UseConcMarkSweepGC	並行GC		@<code>{GenCollectedHeap}
-XX:UseG1GC		G1GC		@<code>{G1CollectedHeap}
//}

上記を見ていただくとわかるとおり、GCアルゴリズムとVMヒープクラスの対応については特に明確なルールがありません。
@<code>{GenCollectedHeap}は複数のGCアルゴリズムから利用されますが、@<code>{G1CollectedHeap}はG1GCしか利用しません。

また、@<code>{GenCollectedHeap}という名前から「世代別GCアルゴリズムはこのヒープを利用するのだな」と推測するのは間違いです。
HotpotVMの並列GCやG1GCは@<code>{GenCollectedHeap}を利用しないにも関わらず世代別のアルゴリズムです。
クラス名には惑わされないようにしましょう。

個人的には@<code>{GenCollectedHeap}という名前は一般的すぎていかがなものかとも思います。
どうせやるならVMヒープクラスは全部GCアルゴリズムに1対1に対応させるべきでしょう。
このあたりの設計は長い歴史の上でツギハギされた匂いがして憎めないところですね。


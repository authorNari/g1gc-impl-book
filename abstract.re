= VMヒープの抽象化

HotspotVMではさまざまなGCアルゴリズムを選択できる機能があります。
Javaの起動オプションで「-XX:UseParallelGC」などと指定するアレのことです。
GCはそれぞれ管理するヒープの形状が異なり、また当然ですがGCアルゴリズム自体も異なります。
本章ではHotspotVMのヒープやGCの抽象化がどのように設計されているかを見ていきます。

== VMヒープのインタフェース

@<img>{vm_heap_interface}にVMヒープのインタフェースイメージを示します。

//image[vm_heap_interface][VMヒープがVMに公開するインタフェースのイメージ]

VMヒープはVMに対して主に次の3種類のインタフェースを公開します。

 1. オブジェクトの割り当て
 2. 明示的なGCの実行
 3. オブジェクトの位置や形状に依存した処理

1.は、VMがVMヒープに対してオブジェクトの種類を指定すると、VMヒープの内部に割り当てたオブジェクトの実体が帰ってくるインタフェースです。

2.は、VMがVMヒープに対して明示的にGC実行要求を出すと、VMヒープ内部でGCを実行するインタフェースです。

VMはVMヒープ内のオブジェクトの位置や形状がわかりません。そのため、3.が必要になります。
具体的には、VMヒープ内の全オブジェクトに指定した関数を適用する、あるオブジェクト内の全フィールドに指定した関数を適用する、あるポインタが割り当てられたオブジェクトか確認する、といったインタフェース群が定義されています。

上記のインタフェースさえ守れていれば、VMヒープの内部実装は好きに変更できます。
そのため、GCアルゴリズムの切り替えが可能になっています。
では、VMはVMヒープの内部実装をまったく意識しないのか、というとそういうわけでもありません。
VM内部では一部の処理がGCの種類などよって条件分岐しています。
とはいえ、基本的には上記のインタフェースを利用してVMは実装されています。

== VMヒープ内の全体像

TODO: 全体像を示す
要求
- オブジェクトの要求
- GCの実行
- オブジェクトの位置や形状に依存した処理
  - ヒープ上のオブジェクトを全走査するなど

== CollectedHeapクラス

HotspotVMにはVMヒープを表現するクラスが定義されています。

//image[collected_heap_hierarchy][CollectedHeapクラスの継承関係]

@<img>{collected_heap_hierarchy}に示す通り、VMヒープは@<code>{CollectedHeap}という抽象的なクラスで統一的に扱われます。
@<code>{CollectedHeap}クラスはVMヒープの形状によって子クラスに派生し、この子クラスがVMヒープの実体となります。

=== GCを指定するOpenJDK7の起動オプション

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


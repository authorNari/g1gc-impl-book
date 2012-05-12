= オブジェクト管理機能

HotspotVMではさまざまなGCアルゴリズムを選択できる機能があります。
Javaの起動オプションで「@<code>{-XX:+UseParallelGC}」などと指定するアレのことです。
GCはそれぞれ管理するヒープの形状が異なり、また当然ですがGCアルゴリズム自体も異なります。
本書ではHotspotVMのヒープやGCをまとめて@<b>{オブジェクト管理機能}と呼ぶこととし、オブジェクト管理機能の全体像を最初に見ていきます。

== オブジェクト管理機能のインタフェース

@<img>{vm_heap_interface}にオブジェクト管理機能のインタフェースイメージを示します。

//image[vm_heap_interface][オブジェクト管理機能がVMに公開するインタフェースのイメージ]

オブジェクト管理機能はVMに対して主に次の3種類のインタフェースを公開します。

 1. オブジェクトの割り当て
 2. 明示的なGCの実行
 3. オブジェクトの位置や形状に依存した処理

1.は、VMがオブジェクト管理機能に対してオブジェクトの種類を指定すると、VMヒープの内部に割り当てたオブジェクトの実体が帰ってくるインタフェースです。

2.は、VMがオブジェクト管理機能に対して明示的にGC実行要求を出すと、内部でGCを実行するインタフェースです。

VMはVMヒープ内のオブジェクトの位置や形状がわかりません。そのため、3.が必要になります。
具体的には、VMヒープ内の全オブジェクトに指定した関数を適用する、あるオブジェクト内の全フィールドに指定した関数を適用する、あるポインタが割り当てられたオブジェクトか確認する、といったインタフェース群が定義されています。

上記のインタフェースさえ守れていれば、オブジェクト管理機能の内部実装は好きに変更できます。
そのため、異なるGCアルゴリズム実装を追加していくことが可能です。

基本的には上記のインタフェースを利用してVMは実装されていますが、VMはオブジェクト管理機能の内部実装をまったく意識しないわけでもありません。
例外的な部分はあり、VM内部では一部の処理がGCの種類などよって条件分岐しています。

== オブジェクト管理機能内の全体像

@<img>{vm_heap_internal}にオブジェクト管理機能内の全体像を示します。

//image[vm_heap_internal][オブジェクト管理機能内の全体像。@<code>{CollectedHeap}クラスがインタフェースとなる。@<code>{CollectedHeap}クラスは@<code>{CollectorPolicyクラス}内の設定値を参考に方針を決定する。@<code>{CollectedHeap}クラスは各GCクラスに対してヒープ内のGCを要求する。]

まず、登場人物を簡単に説明しましょう。
@<code>{CollectedHeap}というクラスはオブジェクトを割り当てるVMヒープを管理します。
また、オブジェクト管理機能のインタフェースとして機能し、@<code>{CollectorPolicy}クラスの情報を参照して適切な処理を実行します。

@<code>{CollectorPolicy}クラスはオブジェクト管理機能全体の方針（Policy）を定義するクラスです。
このクラスはオブジェクト管理機能に関わる設定値を保持しており、例えば、ユーザが起動時にオプションで指定した値（GCアルゴリズムなど）はこのクラスが管理します。

各GCクラスはVMヒープ内のゴミオブジェクトを解放する役割を持ち、
主に@<code>{CollectedHeap}クラスに利用されます。
アルゴリズムによってGCのクラスはかなり変化しますので、ここでは「各GCクラス」と表現しています。

@<img>{vm_heap_internal}のように、VMからオブジェクト割り当て要求が行われた場合は、まず@<code>{CollectedHeap}クラスが要求を受け、@<code>{CollectorPolicy}クラスの方針にしたがって、内部のメモリ空間にあるオブジェクトを割り当てます。
もし、メモリ空間に空きがなければ適切な各GCクラスを使ってGCを実行します。

== CollectedHeapクラス

VMヒープを表現する@<code>{CollectedHeap}クラスを詳しく見ていきましょう。

//image[collected_heap_hierarchy][@<code>{CollectedHeap}クラスの継承関係]

@<img>{collected_heap_hierarchy}に示す通り、VMヒープは@<code>{CollectedHeap}という抽象的なクラスで統一的に扱われます。
@<code>{CollectedHeap}クラスはVMヒープの形状によって子クラスに派生し、この子クラスがVMヒープの実体となります。

=== OpenJDK7の起動オプションとVMヒープクラス

@<table>{java_options_vm_heap}にOpenJDK7のGCを指定する起動オプションと利用されるVMヒープクラスの対応表を示します。

//table[java_options_vm_heap][起動オプションと利用するVMヒープクラス]{
起動オプション		GCアルゴリズム		VMヒープクラス
---------------------------------------------------------------
-XX:UseSerialGC		逐次GC		@<code>{GenCollectedHeap}
-XX:UseParallelGC	並列GC		@<code>{ParallelScavengeHeap}
-Xincgc			インクリメンタルGC	@<code>{GenCollectedHeap}
-XX:UseConcMarkSweepGC	並行GC		@<code>{GenCollectedHeap}
-XX:UseG1GC		G1GC		@<code>{G1CollectedHeap}
//}

上記を見ていただくとわかるとおり、GCアルゴリズムとVMヒープクラスの対応については特に明確なルールがありません。
@<code>{GenCollectedHeap}は複数のGCアルゴリズムから利用されますが、@<code>{G1CollectedHeap}はG1GCしか利用しません。

注意して欲しいのは、@<code>{GenCollectedHeap}という名前から「世代別GCアルゴリズムはすべてこのヒープを利用するのだな」と推測してはいけないという点です。
HotpotVMの並列GCやG1GCは@<code>{GenCollectedHeap}を利用しないにも関わらず世代別のアルゴリズムです。
クラス名には惑わされないようにしましょう。

個人的にこのあたりはクラスの粒度が揃っていないように感じます。特に@<code>{GenCollectedHeap}はカバーする範囲が広すぎます。
VMヒープクラスはGCアルゴリズムに1対1に対応させた方がより見通しがよくなるでしょう。

== CollectorPolicyクラス

次に、オブジェクト管理機能の方針を定義する@<code>{CollectorPolicy}クラスを詳しく見ていきましょう。

//image[collected_policy_hierarchy][@<code>{CollectorPolicy}クラスの継承関係]

@<img>{collected_policy_hierarchy}に示す通り、方針は@<code>{CollectorPolicy}という抽象的なクラスで統一的に扱われます。
@<code>{CollectorPolicy}クラスはオブジェクト管理機能の方針によって子クラスに派生します。

=== 起動オプションとCollectorPolicyクラス

@<table>{java_options_policy}にOpenJDK7のGCを指定する起動オプションと利用される@<code>{CollectorPolicy}の子クラスの対応表を示します。

//table[java_options_policy][起動オプションと利用する方針]{
起動オプション		方針
---------------------------------------------------------------
-XX:UseSerialGC		@<code>{MarkSweepPolicy}
-XX:UseParallelGC	@<code>{GenerationSizer}
-Xincgc			@<code>{ConccurentMarkSweePolicy} （@<code>{CMSIncrementalMode=true}）
-XX:UseConcMarkSweepGC	@<code>{ConccurentMarkSweePolicy}
-XX:UseG1GC		@<code>{G1CollectorPolicy_BestRegionsFirst}
//}

@<code>{CollectorPolicy}には起動オプションで指定したオブジェクト管理機能に必要な情報が何らかの形で格納されています。
上記のGCを指定する起動オプション以外の代表的な例だと、@<code>{-Xms}で指定する初期ヒープサイズや、@<code>{-Xmx}で指定する最大ヒープサイズが挙げられます。
また、指定したGCに対する細かな設定値、例えばG1GCの@<code>{MaxGCPauseMillis}（最大停止時間）などの情報も@<code>{CollectorPolicy}に格納されます。

@<code>{CollectedHeap}は@<code>{CollectorPolicy}に格納された情報を参考にして、自らの実行方針を決定し、適切な処理を実行します。

== 各GCクラス

各GCクラスは@<code>{CollectedHeap}によってVMヒープ内部のゴミ収集に利用されます。
それぞれのGCクラスは、共通のインタフェースを持っておらず、かなり自由にクラスが定義されています。
VMヒープのゴミ収集という達成するという目的だけは一致していますが、GCアルゴリズムによって実装はバラバラで一貫性がありません。
ただ、このように自由にクラスを定義しても、@<code>{CollectedHeap}で適切なGCクラスを選択するため問題ありません。

筆者の推測では、どのようなGCアルゴリズムであっても柔軟に追加できるように自由度を高めているのだと思います。
実際、G1GCというかなり特殊なアルゴリズムのGCがOpenJDK7にて開発され、特に問題なく導入されています。

本書ではG1GCで利用されるGCクラス群だけを説明していきます。
他のGCクラス群を読みたい場合は、@<table>{java_options_vm_heap}と@<table>{java_options_policy}内の対応する@<code>{CollectedHeap}・@<code>{CollectorPolicy}を手がかりに、ソースコードを追ってみることをオススメします。

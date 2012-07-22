= さらに勉強したい人へ

本書を読んだ後、さらに深くGCを勉強したい、また、HotspotVMの中身も勉強したい、という人へ向けて、いくらか私が読んだ本や論文、参考にした記事を紹介したいと思います。

まず、HotspotVMのほかのGCのアルゴリズムも学んでみたいぞ！　という方には思いっきりそのままの論文である以下をオススメします。いわゆるCMSと呼ばれているGCですね。

 * Tony Pnntezis, David Detlefs: A Generational Mostly-concurrent Garbage Collector. ISMM 2000.

また、ParallelGCの方を読みたければ@<chap>{mark}のコラムにも書いたタスクスティーリングを勉強することをオススメします。
上記を読んで概念がわかると、HotspotVMのソースコードを実際にあたったときに理解しやすいでしょう。

私自身、Javaの仮想マシンの仕様があんまりわかっておらず、理解に苦労する点がありました。
具体的にはスタックマップの辺りなのですが…。
そこで以下の本を読んで仮想マシンの仕様を勉強しました。
ほんとうによく書かれている本で大変参考になりました。というか単純に面白い本！

  * Tim London, Frank Yellin／村上雅章翻訳: Java仮想マシン仕様 第2版 （ピアソンエデュケーション, 2001）

JIT絡みの話は正直ソースコードだけ読んでもわけがわからなかったので…。
以下のページでだいたいの概念をつかんでからソースコードを読みました。

 * nothingcosmos著: @<href>{http://nothingcosmos.github.com/OpenJDKOverview/index.html, OpenJDK Internals 1.0 documentation} （2011）

あとは以下の論文にもJITやスタックマップ（論文中では「oop map」）の生成タイミングや、セーフポイントの話がのっており参考になりました。

 * Thomas Kotzmannら著: David CoxDesign of the Java HotSpotTM Client Compiler for Java 6. (TACO, 2008) Vol. 5, No. 1, Article 7.

もし興味を持たれた方は上記の資料をあさりながら、またソースコードと格闘してみてください！

ではでは。

//flushright{
2012年5月13日 中村成洋
//}

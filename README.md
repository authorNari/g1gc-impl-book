# 徹底解剖「G1GC」実装編（執筆中）

本書はOpenJDK7のG1GCの実装と、それに関連する技術を解説します。

<img src="http://www.narihiro.info/g1gc-impl-book/images/cover.jpg" title="表紙" alt="表紙" style="border:solid 1px;">

## [目次](http://www.narihiro.info/g1gc-impl-book/)

 * [スポンサーのみなさま](http://www.narihiro.info/g1gc-impl-book/sponsor.html)
 * [はじめに](http://www.narihiro.info/g1gc-impl-book/preface.html)
 * 1.[準備](http://www.narihiro.info/g1gc-impl-book/prepare.html)
 * 2.[アロケータ](http://www.narihiro.info/g1gc-impl-book/alloc.html)
 * 3.[ヒープ構造](http://www.narihiro.info/g1gc-impl-book/heap.html)
 * 4.[オブジェクト構造](http://www.narihiro.info/g1gc-impl-book/object.html)
 * 5.[正確なGCへの道](http://www.narihiro.info/g1gc-impl-book/precise.html)
 * 6.[HotspotVMのスレッド管理](http://www.narihiro.info/g1gc-impl-book/vm_thread.html)
 * 7.[スレッドの排他制御](http://www.narihiro.info/g1gc-impl-book/lock.html)
 * [参考文献](http://www.narihiro.info/g1gc-impl-book/bib.html)

以下から（ある時点で）最新のebookをダウンロードできます。

 * [徹底解剖「G1GC」実装編-20111225.epub](http://www.narihiro.info/ebook/g1gc-impl-20111225.epub)
 * [徹底解剖「G1GC」実装編-20111225.mobi](http://www.narihiro.info/ebook/g1gc-impl-20111225.mobi)
 * [徹底解剖「G1GC」実装編-20111225.pdf](http://www.narihiro.info/ebook/g1gc-impl-20111225.pdf)

## 「ある程度寄付が溜まったら本気出す」
**目標に達しましたので寄付の受付を終了しています**

本書はご覧の通り**（執筆中）**となっており、半分まで書いたところで無料公開しました。続きを読みたい方はぜひ寄付をしてください（ﾁﾗｯﾁﾗｯ

寄付には以下をご利用ください。Paypalが使えて、匿名でも寄付できます。20万円を目標に設定しており、達成したら寄付の受付を終了し、残りの執筆に着手します。

**寄付が目標額に達成し、後半の執筆が完了しても本書のすべてのコンテンツは無料で公開し続けます。**

[Pledgie — Donate To: "Book: Implementation of G1GC"](http://www.pledgie.com/campaigns/16436)

<a href='http://www.pledgie.com/campaigns/16436'><img alt='Click here to lend your support to: Book: Implementation of G1GC and make a donation at www.pledgie.com !' src='http://www.pledgie.com/campaigns/16436.png?skin_name=chrome' border='0' /></a>

※寄付いただいたみなさまの名前は[スポンサーのみなさま](http://www.narihiro.info/g1gc-impl-book/sponsor.html)に随時追加します。

みなさまの温かいご支援をおまちしております ﾟ+.(･ω･)ﾟ+.ﾟ

[補足:寄付方式にした意図](http://d.hatena.ne.jp/authorNari/20111226/1324892029)

## 間違いの指摘・感想など

[authorNari/g1gc-impl-book - GitHub](https://github.com/authorNari/g1gc-impl-book/)にissue登録するなり、コメントなり、pull requestするなり、ご自由にどうぞ！！

## TODO
以下について今後書きたいと思っています。

 * GCスレッド周りの解説
 * JITと正確なGCの解説
 * セーフポイントの解説
 * 並行マーキングの解説
 * 退避の解説
 * 退避時間予測の解説
   * 減衰など
 * GCアルゴリズムの切り替え方法の解説
   * ライトバリアを動的に切り替えても早い理由（たぶんJIT）

## ライセンス
本書籍は[CC BY-SA 2.1](http://creativecommons.org/licenses/by-sa/2.1/jp/)とします。

## 著者について

**中村成洋, nari, @nari3, id:authorNari**

![中村成洋](http://1.gravatar.com/avatar/9f859654c118bcd2f67cc763baf0de7a?size=150 "中村成洋")

株式会社ネットワーク応用通信研究所に勤務。ただのGC好き。以下は今までの著作物。

* [『ガベージコレクションのアルゴリズムと実装』](http://amazon.co.jp/o/ASIN/4798025623/authornari-22)
* [徹底解剖「G1GC」 アルゴリズム編](http://tatsu-zine.com/books/g1gc)

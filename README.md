# 徹底解剖「G1GC」実装編（β版）

本書はOpenJDK7のG1GCの実装と、それに関連する技術を解説します。

<img src="http://www.narihiro.info/g1gc-impl-book/images/cover.jpg" title="表紙" alt="表紙" style="border:solid 1px;">

## [目次](http://www.narihiro.info/g1gc-impl-book/)

 * [スポンサーのみなさま](http://www.narihiro.info/g1gc-impl-book/sponsor.html)
 * [はじめに](http://www.narihiro.info/g1gc-impl-book/preface.html)
 * 1.[準備](http://www.narihiro.info/g1gc-impl-book/prepare.html)
 * 2.[オブジェクト管理機能](http://www.narihiro.info/g1gc-impl-book/abstract.html)
 * 3.[アロケータ](http://www.narihiro.info/g1gc-impl-book/alloc.html)
 * 4.[ヒープ構造](http://www.narihiro.info/g1gc-impl-book/heap.html)
 * 5.[オブジェクト構造](http://www.narihiro.info/g1gc-impl-book/object.html)
 * 6.[HotspotVMのスレッド管理](http://www.narihiro.info/g1gc-impl-book/vm_thread.html)
 * 7.[スレッドの排他制御](http://www.narihiro.info/g1gc-impl-book/lock.html)
 * 8.[GCスレッド（並列編）](http://www.narihiro.info/g1gc-impl-book/gc_thread_par.html)
 * 9.[GCスレッド（並行編）](http://www.narihiro.info/g1gc-impl-book/gc_thread_con.html)
 * 10.[並行マーキング](http://www.narihiro.info/g1gc-impl-book/mark.html)
 * 11.[退避](http://www.narihiro.info/g1gc-impl-book/evac.html)
 * 12.[予測とスケジューリング](http://www.narihiro.info/g1gc-impl-book/scheduling.html)
 * 13.[正確なGCへの道](http://www.narihiro.info/g1gc-impl-book/precise.html)
 * 14.[ライトバリアのコスト](http://www.narihiro.info/g1gc-impl-book/wbarrier.html)
 * [さらに勉強したい人へ](http://www.narihiro.info/g1gc-impl-book/next.html)
 * [その他参考文献](http://www.narihiro.info/g1gc-impl-book/bib.html)

以下から（ある時点で）最新のebookをダウンロードできます。

 * [徹底解剖「G1GC」実装編-20120725.epub](http://www.narihiro.info/ebook/g1gc-impl-20120725.epub)
 * [徹底解剖「G1GC」実装編-20120725.mobi](http://www.narihiro.info/ebook/g1gc-impl-20120725.mobi)
 * [徹底解剖「G1GC」実装編-20120725.pdf](http://www.narihiro.info/ebook/g1gc-impl-20120725.pdf)

## 謝辞

本書はスポンサーのみなさまの金銭的支援によって執筆されました。

[スポンサーのみなさま](http://www.narihiro.info/g1gc-impl-book/sponsor.html)

ありがとうございます！！

<a href='http://www.pledgie.com/campaigns/16436'><img alt='Click here to lend your support to: Book: Implementation of G1GC and make a donation at www.pledgie.com !' src='http://www.pledgie.com/campaigns/16436.png?skin_name=chrome' border='0' /></a>

## 間違いの指摘・感想など

[authorNari/g1gc-impl-book - GitHub](https://github.com/authorNari/g1gc-impl-book/)にissue登録するなり、コメントなり、pull requestするなり、ご自由にどうぞ！！

## ライセンス
本書籍は[CC BY-SA 2.1](http://creativecommons.org/licenses/by-sa/2.1/jp/)とします。

## 著者について

**中村成洋, nari, @nari3, id:authorNari**

![中村成洋](http://1.gravatar.com/avatar/9f859654c118bcd2f67cc763baf0de7a?size=150 "中村成洋")

ただのGC好き。以下は今までの著作物。

* [『ガベージコレクションのアルゴリズムと実装』](http://amazon.co.jp/o/ASIN/4798025623/authornari-22)
* [徹底解剖「G1GC」 アルゴリズム編](http://tatsu-zine.com/books/g1gc)

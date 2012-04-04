= 並行マーキング

本章ではG1GCの並行マーキングの実装を解説していきます。
本章を読む前に『アルゴリズム編 2.並行マーキング』を読むことをオススメします。

== 並行マーキングの全体像

まずは、並行マーキングの全体像を把握していきましょう。

=== 実行ステップ
『アルゴリズム編』の振り返りにはなりますが、はじめに並行マーキングの実行ステップについて簡単に説明しておきます。

並行マーキングは大まかに分けて次の5つのフェーズに別れています。

 1. 初期マークフェーズ
 2. 並行マークフェーズ
 3. 最終マークフェーズ
 4. 生存オブジェクトカウント
 5. 後始末

1.はルートスキャンをおこなうフェーズです。
このフェーズはセーフポイントで実行されます。

2.は1.によってマークされたオブジェクトをスキャンするフェーズです。
このフェーズはミューテータと並行、かつ複数のスレッドで並列に実行されます。

3.は2.でマークしきれなかったオブジェクトをスキャンするフェーズです。
このフェーズもセーフポイントで実行され、かつ複数のスレッドで並列に実行されます。

4.は各リージョンのマークが付いているオブジェクトのバイト数をカウントするフェーズです。
このステップはミューテータと並行、かつ複数のスレッドで並列に実行されます。

5.はマークフェーズの後始末をして次回のマークフェーズに備えるフェーズです。
このフェーズもセーフポイントで実行され、かつ複数のスレッドで並列に実行されます。

並行マーキングは上記の5つのフェーズを1サイクルとして、必要なときに繰り返し実行します。

=== ConcurrentMarkクラス
並行マーキングの各処理は@<code>{ConcurrentMark}というクラスに実装されています。
@<code>{ConcurrentMark}クラスの定義を簡単に見てみましょう。

//source[share/vm/gc_implementation/g1/concurrentMark.hpp]{
359: class ConcurrentMark: public CHeapObj {

375:   ConcurrentMarkThread* _cmThread;
376:   G1CollectedHeap*      _g1h;
377:   size_t                _parallel_marking_threads;

392:   CMBitMap                _markBitMap1;
393:   CMBitMap                _markBitMap2;
394:   CMBitMapRO*             _prevMarkBitMap;
395:   CMBitMap*               _nextMarkBitMap;
//}

375行目の@<code>{_cmThread}には並行マーキングスレッドを保持し、376行目の@<code>{_g1h}はG1GC用のVMヒープを保持します。

377行目の@<code>{_parallel_marking_threads}には並列マーキングで使用するスレッド数が格納されます。

392・393行目にはVMヒープに対応したビットマップの実体が割り当てられます。
@<code>{CMBitMap}クラスについてはTODOで詳しく説明します。

394行目の@<code>{_prevMarkBitMap}は@<code>{_markBitMap1}、もしくは@<code>{_markBitMap2}のいずれかを指しています。
395行目の@<code>{_nextMarkBitMap}も同じです。
そして、@<code>{_prevMarkBitMap}が指す方がVMヒープ全体の@<code>{prev}ビットマップ、@<code>{_nextMarkBitMap}が指す方が@<code>{next}ビットマップになります。

=== ConcurrentMarkThreadクラス
並行マーキングスレッドは@<code>{ConcurrentMarkThread}クラスに実装されています。
このクラスは@<code>{CuncurrentGCThread}クラスを親に持っており、インスタンスを作った段階でスレッドが起動します。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.hpp]{
36: class ConcurrentMarkThread: public ConcurrentGCThread {

49:   ConcurrentMark*                  _cm;
50:   volatile bool                    _started;
51:   volatile bool                    _in_progress;
//}

@<code>{ConcurrentMarkThread}は49行目にあるように、メンバ変数として@<code>{ConcurrentMark}を持ちます。

50行目の@<code>{_started}は並行マーキングが実行開始状態にあるかを示すフラグです。
51行目の@<code>{_in_progress}は並行マーキングが実際に実行中であるかを示すフラグです。

=== 並行マーキングの実行タイミング

=== 並行マーキングスレッドのrun()

== ステップ1―初期マークフェーズ

=== CMBitMapに対するマーク

== ステップ2―並行マークフェーズ

=== エントリポイント

=== セーフポイントの停止

=== SATB

== ステップ3―最終マークフェーズ

== ステップ4―後始末

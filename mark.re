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

並行マーキングは上記の5つのフェーズを1サイクルとして、必要なときに実行しています。

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

50行目の@<code>{_started}は並行マーキングに対して実行開始要求があるかを示すフラグです。
51行目の@<code>{_in_progress}は並行マーキングが実際に実行中であるかを示すフラグです。

=== 並行マーキングの実行開始
並行マーキングスレッドは以下のように、起動してすぐに@<code>{sleepBeforeNextCycle()}を呼び出します。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
93: void ConcurrentMarkThread::run() {

103:   while (!_should_terminate) {

105:     sleepBeforeNextCycle();
//}

@<code>{sleepBeforeNextCycle()}は名前の通り次回のサイクルの前まで待ち状態にするメンバ関数です。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
329: void ConcurrentMarkThread::sleepBeforeNextCycle() {

334:   MutexLockerEx x(CGC_lock, Mutex::_no_safepoint_check_flag);
335:   while (!started()) {
336:     CGC_lock->wait(Mutex::_no_safepoint_check_flag);
337:   }
338:   set_in_progress();
339:   clear_started();
340: }
//}

334行目で@<code>{CGC_lock}というグローバルなミューテータをロックし、336行目で待ち状態になります。
もし、並行マーキングスレッドの待ち状態が解かれると、338行目で@<code>{_in_progress}を@<code>{true}にし、339行目で@<code>{_started}を@<code>{false}にします。

さて、並行マーキングに次のサイクルを実行させたければ、@<code>{_started}を@<code>{true}にして、@<code>{CGC_lock}に@<code>{notify()}しないといけません。
それをやるのが@<code>{G1CollectedHeap}の@<code>{doConcurrentMark()}です。

//source[share/vm/gc_implementation/g1/g1CollectedHeap.cpp]{
3047: void
3048: G1CollectedHeap::doConcurrentMark() {
3049:   MutexLockerEx x(CGC_lock, Mutex::_no_safepoint_check_flag);
3050:   if (!_cmThread->in_progress()) {
3051:     _cmThread->set_started();
3052:     CGC_lock->notify();
3053:   }
3054: }
//}

3051行目で@<code>{_started}を@<code>{true}にし、すぐ後に@<code>{CGC_lock}に対して@<code>{notify()}しています。
また、3050行目のif文によって、並行マーキングの実行中に@<code>{set_started()}が呼ばれることはありません。

並行マーキングの実行開始タイミングは退避の直後のみです。
そのため、@<code>{doConcurrentMark()}の呼び出しは「退避の直後」にしかありません。

=== 並行マーキングのサイクル
@<code>{ConcurrentMarkThread}の@<code>{run()}の@<code>{while}ループに並行マーキングの1サイクルが実装されています。
@<code>{run()}は200行以上もある大きな関数であるため、説明に必要な部分だけを抜き出しました。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
93: void ConcurrentMarkThread::run() {

103:   while (!_should_terminate) {

105:     sleepBeforeNextCycle();
106:     {

             /* 2. 並行マークフェーズ */
143:         if (!cm()->has_aborted()) {
144:           _cm->markFromRoots();
145:         }

             /* 3. 最終マークフェーズ */
150:         if (!cm()->has_aborted()) {

165:           CMCheckpointRootsFinalClosure final_cl(_cm);
166:           sprintf(verbose_str, "GC remark");
167:           VM_CGC_Operation op(&final_cl, verbose_str);
168:           VMThread::execute(&op);
169:         }

           /* 4. 生存オブジェクトカウント */
190:       if (!cm()->has_aborted()) {

198:         _sts.join();
199:         _cm->calcDesiredRegions();
200:         _sts.leave();

211:       }

           /* 5. 後始末 */
218:       if (!cm()->has_aborted()) {

226:         CMCleanUp cl_cl(_cm);
227:         sprintf(verbose_str, "GC cleanup");
228:         VM_CGC_Operation op(&cl_cl, verbose_str);
229:         VMThread::execute(&op);
230:       }

           /* nextビットマップクリア */
287:       _sts.join();
288:       _cm->clearNextBitmap();
289:       _sts.leave();
290:     }

299:   }

302:   terminate();
303: }
//}

上記の中にステップ1の初期マークフェーズはありません。
なぜかというと、初期マークフェーズは実は退避と一緒にやってしまう処理だからです。
退避でもオブジェクトコピーのためにルートスキャンしないといけません。
同じ処理を並行マーキングでやるのも無駄ですので、退避のルートスキャンと一緒にマークも付けてしまいます。

143〜145行目がステップ2の並行マーキングフェーズにあたる部分です。
144行目の@<code>{markFromRoots()}が初期マークフェーズでマークしたオブジェクトをスキャンするメンバ関数になります。

150〜169行目がステップ3の最終マークフェーズです。
VMオペレーションを使って処理を実行します。

190〜211行目がステップ4の生存オブジェクトカウントです。
こちらは@<code>{SuspendableThreadSet}を使っていることがわかります。

218〜230行目までがステップ5の後始末です。
こちらもVMオペレーションを使っています。

287〜289行目までは@<code>{next}ビットマップをクリアする処理です。
次回の並行マーキングに利用するビットマップの準備ですね。

各フェーズのif文の条件である@<code>{has_aborted()}は、並行マーキングが何らかの理由で中断したい場合に@<code>{true}を返します。
ほとんどの理由は並行マーキングのサイクル実行中に退避が発生することです。
退避がおこるとオブジェクト自体が移動するため、マークを付け直さないといけません。
そのため、中断時には各フェーズをスキップし、マークをクリアする処理のみが実行されます。

== ステップ1―初期マークフェーズ

=== CMBitMapに対するマーク

== ステップ2―並行マークフェーズ

=== エントリポイント

=== セーフポイントの停止

=== SATB

== ステップ3―最終マークフェーズ

== ステップ4―後始末

= 予測とスケジューリング
『アルゴリズム編 4.5 並行マーキングの停止処理』で次回の並行マーキングの停止処理にかかる時間は過去の停止時間の履歴から予測する、と述べました。
本章では過去の履歴から次の数値をどのように予測するのかという点について説明します。

その後、『アルゴリズム編 4.4 停止のスケジューリング』で述べたGCによる停止タイミングのスケジューリングの実装を見ていきます。

== 履歴に基づいた予測
停止時間の履歴から次の停止時間を求めるというは、言い換えれば「過去の数値データを基に未来の数値を予測する」ということです。
HotspotVMでは平均と標準偏差を使って、未来の数値を予測します。

=== 相加平均・分散・標準偏差
HotspotVMでは分散や標準偏差といったテクニックを利用します。
まずはこれらの用語について学んでおきましょう。

あるクラスのA、B、Cという三人のテスト結果が返ってきました。
結果は次のとおりです。

 * A: 50点
 * B: 70点
 * C: 90点

さて、A、B、Cのテスト結果の「平均」は何になるでしょう？　
これは簡単ですね。次のような計算で求められます。

//list[avg][A,B,Cの平均値]{
 (50+60+90) / 3 = 70
//}

上記のように単純に各要素を足して要素数で割ることを@<b>{相加平均}と呼びます。
これはもっとも親しみのある「平均」ではないでしょうか。

では、次にA、B、Cが基準値からどのくらいバラつきがあるのかみたいとします。
このバラつきを示す目安の値を@<b>{標準偏差}と呼びます。
今回は基準値を相加平均値として計算してみましょう。

各データのバラつきを見るためには、まずは各データが基準となる平均値からどれくらい離れているかを見る必要があります。
単純に考えると「各データ - 平均値」とすれば差分は求められるわけですから、それらを足し合わせてデータ数で割れば全体のバラつきは出てきそうです。
ちょっと試してみましょう。

//list[simple_guess][誤ったバラつきの確認]{
 ((50 - 70) + (70 - 70) + (90 - 70)) / 3 = 0
//}

@<code>{0}になってしまいました…。データのバラつきはないということでしょうか。
でも、A、B、Cを見ると明らかにデータはバラついています。

「各データ - 平均値」としたことの誤りは結果がマイナスになってしまうということです。
マイナスなれば総計するときに差分が相殺される可能性があります。
マイナスになると都合が悪いので、こんどは各値を2乗して総計してみます。

//list[variance][分散の求め方]{
 ((50 - 70)**2 + (70 - 70)**2 + (90 - 70)**2) / 3 = 266
//}

おぉ、@<code>{266}とでましたね。
なんとなくバラついていることがわかります。
このように各データを2乗総和してデータ数で割った値を@<b>{分散}と呼びます。

この値は2乗して求められた値でしたから、平方根の値を求めます。

//list[standard_deviation][標準偏差の求め方 by Ruby]{
Math.sqrt(266).to_i # => 16 小数点以下切り捨て
//}

@<code>{16}と出てきました。この値が先ほど述べた標準偏差です。
標準偏差はバラつきを表す目安です。
標準偏差が大きければ各データはバラついていると判断できます。
もし標準偏差が@<code>{0}の場合はまったくバラつきがないと判断できます。
たとえばこのケースだとA、B、Cのテスト結果がすべて@<code>{70}だったときは、標準偏差が@<code>{0}になります。

=== 減衰平均
HotspotVMは過去の履歴から次の数値を予測します。
たとえばAの過去5回のテスト結果が次のようになっていたとしましょう。

 * 1回目: 30点
 * 2回目: 35点
 * 3回目: 40点
 * 4回目: 42点
 * 5回目: 50点

では、6回目のテストの点数はどのように推測できるでしょうか？

HotspotVMではまず履歴に点数を登録する際に@<b>{減衰平均（Decaying average）}と呼ばれるものを計算していきます。
減衰平均は相加平均とは違い、履歴データが過去になればなるほど平均値に与える影響が少なるなくなる計算方法です。
とりあえずは計算方法を見てみましょう。

//list[decaying_avg][減衰平均の求め方 by Ruby]{
davg = 30
davg = 35 * 0.3 + davg * 0.7
davg = 40 * 0.3 + davg * 0.7
davg = 60 * 0.3 + davg * 0.7
davg = 50 * 0.3 + davg * 0.7
davg.to_i # => 44
//}

履歴に登録する最新の得点を3割、過去の履歴を7割にして加算していきます。
このように計算することで平均値が履歴の古いデータから受ける影響を少なくできます。

もうすこしわかりやすくするために、得点が1点の履歴が10回分あった例を考えてみます。
そうすると相加平均の場合は@<img>{avg_shift}のように平均値が推移します。

//image[avg_shift][相加平均値の推移]

一方、減衰平均の場合は@<img>{davg_shift}のように推移します。

//image[davg_shift][減衰平均の推移]

1個目の値を例外として、過去の値であればあるほど平均値の成分として占める割合が少なくなっています（@<img>{davg_shift}）。
逆に新しい値は常に平均値の3割を占めています。

HotspotVMではこの減衰平均値を次回の数値であると予測します。
たとえば@<list>{decaying_avg}のケースでは「@<code>{44}」を予測数値とします。

履歴情報のデータは過去のものになるにつれて最新のデータに関係なくなっていきます。
ですので、減衰平均のような過去のデータの影響を減らした平均値の求め方がフィットするのでしょう。

=== 減衰偏差値
減衰平均とあわせて@<b>{減衰分散（Decaying variance）}と呼ばれる値も計算していきます。
計算方法を見てみましょう。

//list[decaying_variance][減衰分散の求め方 by Ruby]{
davg = 30
dvar = 0
davg = 35 * 0.3 + davg * 0.7
dvar = ((35 - davg) ** 2) * 0.3 + dvar * 0.7
davg = 40 * 0.3 + davg * 0.7
dvar = ((40 - davg) ** 2) * 0.3 + dvar * 0.7
davg = 42 * 0.3 + davg * 0.7
dvar = ((42 - davg) ** 2) * 0.3 + dvar * 0.7
davg = 50 * 0.3 + davg * 0.7
dvar = ((50 - davg) ** 2) * 0.3 + dvar * 0.7
dvar.to_i # => 40
//}

分散とは基準値からの距離を示す値です。
この場合の基準値は「データを追加した時点の履歴全体の減衰平均値」となります。
減衰平均値は予測値（＝次回の数値と予想されるもの）でしたね。
つまりこの時の分散は「その時の予測値から実際のデータがどのくらい離れているか」を表します。
さらに分散は減衰平均と同じ方法で過去データの影響を徐々に減衰して求めます。

その後、減衰分散の平方根である@<b>{減衰標準偏差（Decaying standard deviation）}を求めます。

//list[decaying_standard_deviation][減衰標準偏差の求め方 by Ruby]{
Math.sqrt(dvar).to_i # => 6
//}

ここで求められた減衰標準偏差は予測値と実際のデータのバラつきを示します。
そのため予測値から実際の値が正負6の範囲でバラつくだろうと予測できるわけです。

=== バラつきを含めた予測
HotspotVMではある程度値がバラつくことを考え、ほとんどのケースで安全側に倒した予測値を計算します。
具体的な計算方法を@<list>{safe_prediction}に示します。

//list[safe_prediction][安全側に倒した予測値]{
バラつきを含めた予測値 = 減衰平均値 + (信頼度/100 * 減衰標準偏差)
//}

ここで@<b>{信頼度}という新しい言葉がでてきます。
信頼度は減衰標準偏差で求められたバラつきの範囲をどの程度信頼するかを示す値です。
たとえば減衰標準偏差が@<code>{6}だった場合、信頼度が100%であればバラつきの範囲を正負@<code>{6}の範囲にします。
もし信頼度が50%の場合は半分の正負@<code>{3}の範囲に狭めます。
HotspotVMにおいてこの信頼度はデフォルト50%で指定されますが、起動時のフラグとして言語利用者が指定することも可能です。

@<list>{safe_prediction}では信頼できる範囲のバラつきの最大値と減衰平均値（予測値）を足し合わせることで、安全側に倒した予測値を求めています。

//list[safe_prediction_of_a][Aのテストの履歴におけるHotspotVMの予測 by Ruby]{
44 + (50/100.0 * 6) # => 47.0
//}

AのテストのケースであればHotspotVMは@<list>{safe_prediction_of_a}のように計算し、「Aは次回のテストで47点を取る」と安全側に倒した予測をします。

==== 注:用語について
上記までの項で紹介した減衰平均・減衰分散・減衰標準偏差という用語はHotspotVMのソースコード中の記述に習って本書でも使用したものです。
一般的な用語ではありませんので注意してください。

=== 履歴の実装
では実際の実装を見ていきましょう。
履歴は次のように@<code>{G1CollectorPolicy}のメンバ変数に保持されます。

//source[share/vm/gc_implementation/g1/g1CollectorPolicy.hpp]{
 86: class G1CollectorPolicy: public CollectorPolicy {

150:   TruncatedSeq* _concurrent_mark_init_times_ms;
151:   TruncatedSeq* _concurrent_mark_remark_times_ms;
152:   TruncatedSeq* _concurrent_mark_cleanup_times_ms;
//}

この@<code>{TruncatedSeq}は@<code>{AbsSeq}クラスを継承した履歴を保持するクラスです。
では、履歴を追加する@<code>{add()}メンバ関数を見てみましょう。

//source[share/vm/utilities/numberSeq.cpp]{
36: void AbsSeq::add(double val) {
37:   if (_num == 0) {
39:     _davg = val;
41:     _dvariance = 0.0;
42:   } else {
44:     _davg = (1.0 - _alpha) * val + _alpha * _davg;
45:     double diff = val - _davg;
46:     _dvariance = (1.0 - _alpha) * diff * diff + _alpha * _dvariance;
47:   }
48: }
//}

@<code>{_davg}が減衰平均値、@<code>{_dvariance}が減衰分散を表します。
@<code>{_alpha}のデフォルト値は@<code>{0.7}です。
つまり、おこなっている処理は@<list>{decaying_variance}の内容と変わりありません。
履歴にデータを追加するたびに上記のメンバ変数が計算されていきます。

次に実際に履歴にデータを追加する箇所を見てみましょう。
たとえば、並行マーキングの初期マークフェーズは次のメンバ関数で追加しています。

//source[share/vm/gc_implementation/g1/g1CollectorPolicy.cpp]{
954: void G1CollectorPolicy::record_concurrent_mark_init_end() {
955:   double end_time_sec = os::elapsedTime();
956:   double elapsed_time_ms = (end_time_sec - _mark_init_start_sec) * 1000.0;
957:   _concurrent_mark_init_times_ms->add(elapsed_time_ms);

961: }
//}

956行目で初期マークフェーズの停止時間を求め、957行目でその時間を@<code>{TruncatedSeq}に追加しています。

=== 予測値の取得
次に予測値を得る部分の処理を見てみましょう。
例として初期マークフェーズの予測値を求めるメンバ関数を次に示します。

//source[share/vm/gc_implementation/g1/g1CollectorPolicy.hpp]{
536:   double predict_init_time_ms() {
537:     return get_new_prediction(_concurrent_mark_init_times_ms);
538:   }
//}

537行目の@<code>{get_new_prediction()}に@<code>{_concurrent_mark_init_times_ms}メンバ変数を渡して、予測値を返します。

この@<code>{get_new_prediction()}の定義は次のようになります。

//source[share/vm/gc_implementation/g1/g1CollectorPolicy.hpp]{
342:   double get_new_prediction(TruncatedSeq* seq) {
343:     return MAX2(seq->davg() + sigma() * seq->dsd(),
344:                 seq->davg() * confidence_factor(seq->num()));
345:   }
//}

@<code>{MAX2()}は引数を比較し大きな数字を返す関数です。
344行目の処理は履歴がまだ充分に得られていないときに利用するものですので説明を割愛し、343行目の処理のみを説明します。

@<code>{davg()}は減衰平均値を返します。
@<code>{sigma()}は言語利用者が与えた信頼度です。
@<code>{dsd()}は減衰標準偏差を返します。
つまり、この処理では@<list>{safe_prediction}で示した安全側に倒した予測値を求めているのです。

== 並行マーキングのスケジューリング
では、『アルゴリズム編 4.4 停止のスケジューリング』で述べたGCによる停止タイミングのスケジューリングの実装について見ていきましょう。
ここは履歴から予測値を得る部分をしっていればかんたんに理解できます。

並行マーキングの停止処理のうち、例として最終マークフェーズを取り上げます。

//source[share/vm/gc_implementation/g1/concurrentMarkThread.cpp]{
93: void ConcurrentMarkThread::run() {

152:             double now = os::elapsedTime();
153:             double remark_prediction_ms =
                   g1_policy->predict_remark_time_ms();
154:             jlong sleep_time_ms =
                   mmu_tracker->when_ms(now, remark_prediction_ms);
155:             os::sleep(current_thread, sleep_time_ms, false);

               /* 最終マークフェーズ実行 */
165:           CMCheckpointRootsFinalClosure final_cl(_cm);
166:           sprintf(verbose_str, "GC remark");
167:           VM_CGC_Operation op(&final_cl, verbose_str);
168:           VMThread::execute(&op);
//}

152行目の@<code>{os::elapsedTime()}はHotspotVMが起動してからの経過時間を返す静的メンバ関数です。
153行目の@<code>{predict_remark_time_ms()}で次に起きる最終マークフェーズにかかる時間の予測値を得ます。
それを@<code>{when_ms()}メンバ関数に渡します。
この@<code>{when_ms()}では『アルゴリズム編 4.4 停止のスケジューリング』で述べた方法を使って適切な実行タイミングまでの時間を返します。
そうして求められた値を155行目の@<code>{os::sleep()}に渡し、並行マークスレッドを適切な実行タイミングまで寝かせます。

並行マーキングのほかの停止処理でも上記と同じように実行タイミングを決定しています。

== 退避のスケジューリング
退避の実行タイミングは『アルゴリズム編 5.8 新世代リージョン数上限決定』で述べたように新世代リージョンの数で決まります。
全新世代GCの計算方法はかなり複雑なのでかんたんな部分的新世代GCの場合だけ見てみましょう。

部分的新世代GCの場合、新世代リージョン数上限はGC単位時間を守れる範囲のなるべく小さい値に設定しなければなりません。
その値は次のメンバ関数で設定します。

//source[share/vm/gc_implementation/g1/g1CollectorPolicy.cpp]{
503: void G1CollectorPolicy::calculate_young_list_min_length() {
504:   _young_list_min_length = 0;
505: 
509:   if (_alloc_rate_ms_seq->num() > 3) {
510:     double now_sec = os::elapsedTime();
511:     double when_ms = _mmu_tracker->when_max_gc_sec(now_sec) * 1000.0;
512:     double alloc_rate_ms = predict_alloc_rate_ms();
513:     size_t min_regions = (size_t) ceil(alloc_rate_ms * when_ms);
514:     size_t current_region_num = _g1->young_list()->length();
515:     _young_list_min_length = min_regions + current_region_num;
516:   }
517: }
//}

まず、511行目の@<code>{when_max_gc_sec()}に現在の経過時間を渡して、次回の停止可能タイミングまでの時間を求めます。
512行目の@<code>{predict_alloc_rate_ms()}は次回の「割り当てたリージョン数/経過時間」というレートを予測するメンバ関数です。

//source[share/vm/gc_implementation/g1/g1CollectorPolicy.hpp]{
379:   double predict_alloc_rate_ms() {
380:     return get_new_prediction(_alloc_rate_ms_seq);
381:   }
//}

@<code>{_alloc_rate_ms_seq}には過去の「割り当てたリージョン数/経過時間」レートの履歴を保持しており、その履歴情報から次の予測値を得ます。

その後、@<code>{calculate_young_list_min_length()}の513行目で求めた予測値と次回の停止可能タイミングまでの時間（msec）をかけて、次回の停止可能タイミング付近までに割り当てられるだろうリージョン数の予測値をだします。
最後の515行目で現在の新世代リージョン数上限とその数字を足して、部分的新世代GC用の新世代リージョン数上限が決定します。

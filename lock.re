= スレッドの排他制御

本章ではスレッド間の共有リソースにアクセスするための排他制御について説明します。
GCで利用するスレッドはオブジェクトを共有リソースとして扱いますので、排他制御が必要になる場面が多々あります。

== 排他制御とは

メモリ領域を共有するスレッドでは、同じアドレスにあるデータを複数のスレッドが同時に読み書きできてしまいます。
ほかのスレッドから割り込みで変更される可能性があるデータに対し、割り込みを想定していないコードを書いてしまった場合は、思わぬところでメモリ破壊が生じてしまい、原因が特定しづらいエラーが発生してしまうかもしれません。

このように単一のリソースに対して、複数のスレッドから同時に処理が実行されるとまずい部分を@<b>{クリティカルセクション}と呼びます。

クリティカルセクションを扱う処理では、スレッド単体でアトミックに一連の処理を実行し、割り込みがないようにほかのスレッドの処理を排除する必要があります。
このようにほかのスレッドを排除し、あるスレッドだけでリソースを独占的に利用させることを@<b>{排他制御}と呼びます。

== ミューテックス（Mutex）

排他制御の単純な実装例としてよく利用されるのが@<b>{ミューテックス（Mutex）}です。これはmutal exclusion（相互排他）の略からきた造語です。

ミューテックスの例え話はいくつもありますが、ここでは武者さんが書かれたトイレの例@<bib>{webdb_can_thread}を取り上げてみます。

//quote{
トイレが1つしかない家に、何人かの家族が住んでいるとします。
トイレの使用にあたっては決まりがあり、ドアプレートの表示が「使用中」のときは中に入ることができず、入りたい人は外で待機します。
「空室」のプレートに当たった人は、これを裏返して「使用中」にすると中に入る権利を得て、独占的にトイレを使えます。
トイレを使い終わった人は「使用中」のプレートを「空室」に戻しますが、ほかの人がこの操作をおこなうことは許されません。
このとき、プレートを「使用中」に替えることをロック（lock）、「空室」に戻すことをアンロック（unlock）、トイレのことをクリティカルセクション（critical section）といいます。
//}

いくら仲のいい家族でも2人同時にトイレを利用しないですよね（きっと）。
ですので、家族ひとりひとりがスレッドだとしたら、トイレがクリティカルセクションにあたるのもうなずけます。

//quote{
このしくみにより、ドアのノックを交わす必要はなくなり、すでに人が入っているところに別の人が入ることもなくなります。
これがミューテックスです。
//}

ミューテックスは排他制御の基本的な実装であり、ミューテックスを土台として様々な排他制御が実装されます。

== モニタ（Monitor）

Javaでは言語自体に@<b>{モニタ（Monitor）}という同期機構が組み込まれています。
そして、HotspotVMの内部の排他制御はほとんどがこのモニタを使っておこなわれます。

早速、モニタの説明に入りたいところですが一つ注意点があります。
Javaで利用されているモニタは実は一般的に知られているモニタとは少しことなります。
ですので、一般的なモニタについて知りたい場合は武者さんの記事@<bib>{webdb_can_thread}を読むことをお勧めします。

=== Javaのモニタ

さて、Javaのモニタについてスノーボードのレンタルショップのたとえ話を持ちだしてみます。
このレンタルショップで扱うスノーボードはすべて同じサイズ・同じデザインとします。
また、店内は狭いため、一度に1人の客しか入ることができません。
もし先に客が入っていた場合は、店の前に行列を作って待ちます。
店内に客がいない場合は行列の先頭1人が店に入ることができます。
店内に入った客は自分にあったスノーボードを借りて店をでます。
もし自分にあったスノーボードがなかった場合は、店に備え付けの待合室で待ちます。

スノーボードを返しに来た客も同様に行列に並びます。
返し終わった客は待合室の客の内1人、もしくは客全員を呼ぶことができます。
呼ばれた客は店内に客がいないときに店内に入ります。
店の前に行列ができていた場合は行列の後ろに並びなおします。
再度入店しても、スノーボードがない場合はまた待合室に入って待ちます。

//image[rentalshop][モニタの例え: 狭いレンタルショップ]

上記はモニタのたとえ話です。
この場合、共有リソースはスノーボードであり、モニタはレンタルショップを指します。
客がスレッドだとすると、スレッドはモニタの中に高々1スレッドしか入れません。
レンタルショップに客が入っている状態は、店自体にロックがかかっている状態と言えます。
客が出ると店がアンロックされ、ほかの客が入れるようになります。
Javaの文化では待合室で待つことを@<b>{Wait}、待合室内の1人を呼ぶことを@<b>{Notify}、全員を呼ぶことを@<b>{NotifyAll}といいます。

=== 一般的なモニタとの違い

別のたとえも少し考えてみましょう。
もし共有しているリソースがレンタルビデオだとしたらどうなるでしょうか。
来た客は対象のビデオがなかった場合に待合室で待ちます。
返却にきた客はビデオ返却後に待合室の客を呼び出しますが、呼び出された客は返されたビデオが自分の待っていたビデオとは限りません。
違った場合はまた待合室に戻ることになります。
このモニタのたとえで無駄な点は、待合室で待っている客が自分の欲しいビデオを店内に伝えられない点です。
もし「私の欲しいビデオはこれです（太郎）」などと店内に張り紙できれば、返却しにきた客が張り紙を見て適切な待ち人を呼べます。
呼ばれた客は、店内に入った後で自分の欲しいビデオがなくてがっかりすることも少なくなるでしょう。
この便利な張り紙のことを@<b>{条件変数（condition variable）}と呼びます。

「Javaのモニタが一般的なモニタと異なる点」というのは実はこの部分で、一般的なモニタにはこの条件変数がありますが、Javaのモニタにはありません。

モニタが管理する共有リソースが例えばビデオのように客の要求に強く依存する場合は張り紙がある方が有利です。
返却しにきた客は待合室にいる適切な客を選ぶことが可能で、呼び出された客も関係ないときに呼ばれることが少なくなります。
一方、Javaのモニタの場合は張り紙がないので、いったん待合室の全員を呼んで、呼び出された客がリソースを判断しなければなりません。

ただし、モニタが管理するリソースがスノーボードの例のように@<b>{客の要求に依存しないもの}であればJavaのモニタでも問題ありません。
ボード自体に個性がないため、待っている客は借りるものは何でもよく、単純に空きがでるのを待っているだけですから、張り紙は必要ないのです。

このように、Javaでは条件変数をなくしたシンプルなモニタを提供しています。

== モニタの実装概要

ここで気になるのはモニタの実装方法です。
ただ、この話題はGCの話から脱線しすぎる予感がしますので、実装の重要な部分をかいつまんで説明します。
また、ここで取り上げる実装方法はHotspotVMの例です。
モニタ実装の一例として捉えてください。

=== スレッドの一時停止・再起動

まず、行列や待合室での一時停止・再起動処理を見てみましょう。
それぞれの処理は次のメンバ関数で実装されています。

 * @<code>{os::PlatformEvent::park()} - 待つ
 * @<code>{os::PlatformEvent::unpark()} - 再起動

parkは「駐車する」、unparkは「発車する」という意味があります。
それぞれのメンバ関数はそれぞれのOS用に実装されていますが、今回はLinuxのものを簡単に見ていきます。

@<code>{park()}では次のように@<code>{pthread_cond_wait()}を利用して待つ処理を実現しています。

//source[os/linux/vm/os_linux.cpp]{
4916: void os::PlatformEvent::park() {

4928:      int status = pthread_mutex_lock(_mutex);

4933:         status = pthread_cond_wait(_cond, _mutex);

4948: }
//}

@<code>{os::PlatformEvent}のインスタンスは@<code>{_cond}と@<code>{_mutex}のメンバ変数を保持しています。
@<code>{_cond}は条件変数、@<code>{_mutex}はミューテックスであり、それぞれPthreadsで利用される変数です。
4928行目で@<code>{pthread_mutex_lock()}を使って、@<code>{_mutex}をロックします。
その後、4933行目で@<code>{pthread_cond_wait()}を使って、現在のスレッドを一時停止状態にします。
@<code>{pthread_cond_wait()}には@<code>{_cond}と、ロック状態の@<code>{_mutex}を指定します。
@<code>{_mutex}は@<code>{pthread_cond_wait()}内部で一時停止状態になった際にアンロックされます。

@<code>{unpark()}は次のように@<code>{pthread_cond_signal()}を利用してスレッドを再起動します。

//source[os/linux/vm/os_linux.cpp]{
5011: void os::PlatformEvent::unpark() {

5028:      int status = pthread_mutex_lock(_mutex);

5034:         pthread_cond_signal (_cond);

5049: }
//}

5034行目で登場する@<code>{pthread_cond_signal()}では引数に取った条件変数で待っている1つのスレッドに対してシグナルを送り、再起動します。
ここでは@<code>{os::PlatformEvent}インスタンスの@<code>{_cond}変数で待っているスレッドに対してシグナルを送ります。

ちなみに、Windowsでは上記とほぼ同じことを@<code>{WaitForSingleObject()}、@<code>{SetEvent()}を利用して実装しています。

@<code>{Thread}クラスは@<code>{os::PlatformEvent}クラスを継承した@<code>{ParkEvent}クラスのインスタンスをメンバ変数として保持しています。

//source[share/vm/runtime/thread.hpp]{
94: class Thread: public ThreadShadow {

       // 内部のMutex/Monitorに利用される
582:   ParkEvent * _MutexEvent ;                    

//}

そのため、@<img>{thread_park_unpark}のように、HotspotVMが管理する1スレッド（@<code>{Thread}インスタンス）の@<code>{_MutexEvent}に対して、@<code>{park()}・@<code>{unpark()}を呼ぶことで、対象のスレッドを一時停止・再起動させることができます。
モニタで説明した「待合室で待つ」「待合室から出る」「行列を作って待つ」などはこの@<code>{park()}・@<code>{unpark()}を利用して実装されます。

//image[thread_park_unpark][1スレッドに対してpark()を呼ぶとスレッドは一時停止する。一時停止中はCPUを無駄に利用しない。unpark()を呼ぶとスレッドは再起動する。]

=== モニタのロック・アンロック

次にモニタのロック・アンロックについて見ていきましょう。
ここからは実装が複雑なので概要だけを紹介します。

モニタの状態の一例を@<img>{monitor_lock_unlock_1}に図示しました。
このモニタでは行列（@<code>{EntryList}）にスレッドB,Cが並んで待っています。
モニタの前には小さな前室（@<code>{OnDeck}）があります。
そして、モニタ内のロックはスレッドAが現在は保持しています。

//image[monitor_lock_unlock_1][モニタの状態の一例。モニタはロックされているため、EntryListのスレッドBはロックを取ることができない。]

まずはロックのフローについて説明しましょう。
ロック取得時、モニタに入っているスレッドがいなければ、すぐにモニタに入りロックを取得します（@<img>{monitor_lock_unlock_1}のスレッドA参照）。
モニタにすでにスレッドが入っていれば、ロックを取得したいスレッドは@<code>{EntryList}に並び、モニタが空くのを待ちます（@<img>{monitor_lock_unlock_1}のスレッドB・C参照）。

次にアンロックのフローについて説明します。
モニタのアンロック時には@<code>{EntryList}にいるスレッドにロックをとらせる処理が入ります。
スレッドAのアンロック時の、手順を具体的に以下に示しました。

 1. モニタをアンロックしたスレッドAが@<code>{EntryList}の先頭を@<code>{OnDeck}に格納
 2. スレッドAは@<code>{OnDeck}のスレッドBを起こす（@<code>{unpark()}）
 3. スレッドBは@<code>{OnDeck}に自分がいるのを確認
 4. スレッドBがモニタに入り、ロックする

=== モニタのWait・Notify・NotifyAll

まずはWaitの実装を考えてみましょう。
モニタ内のスレッドがwaitする場合もモニタをアンロックすることになりますので、基本的には前節のモニタのアンロックの処理と同じになります。
ただし、Waitでは@<code>{park()}を使って自身のスレッドを待たせる処理が必要になります。

次にNotifyAllの実装を考えてみましょう。
@<img>{monitor_wait_notify_1}に複数のスレッドが待合室（@<code>{WaitSet}）で待っている図を示しました。
@<code>{EntryList}にスレッドが並んでおらず、モニタのロックはスレッドAが握っています。

//image[monitor_wait_notify_1][スレッドB,CがWaitSetでpark()された状態になっている。スレッドAはモニタのロックを保持。EntryListは空。]

ここでスレッドAがモニタ内でNotifyAllをおこなったとします。

NotifyAllでは、@<code>{WaitSet}内のスレッドを@<code>{WaitSet}から取り出し、@<code>{unpark()}を呼び出します。
@<code>{WaitSet}にいたスレッドは外に出た後でほぼ同時に動き出します。
その後、@<img>{monitor_wait_notify_2}のように、お互いに競争しながら1つキューを作ります。
これを@<code>{ContentionQueue}と呼びます。
キューを作ったあとはスレッド自身が@<code>{park()}を呼び出し、一時停止状態に入ります。

//image[monitor_wait_notify_2][WaitSetから出たスレッドB,Cは競争し合いながら1つのキューを作る。この際の並び順は競争に勝った順にならんでいく]

最後にスレッドAがモニタをアンロックします。
その際、スレッドAは@<code>{EntryList}の先頭を@<code>{OnDeck}に格納しようと思いますが、@<code>{EntryList}は空であるため、@<code>{ContentionQueue}を@<code>{EntryList}に昇格します。
その後、@<code>{EntryList}の先頭を@<code>{OnDeck}に格納し、@<code>{unpark()}を呼び出します。
以降の処理は以前説明したアンロックの処理と同じです。

//image[monitor_wait_notify_3][モニタをアンロックするスレッドAはContentionQueueをEntryListに昇格し、先頭をOnDeckに格納する]

最後のNotifyの実装ですが、これは@<code>{WaitSet}から呼び出されるのが全スレッドか、1スレッドであるかの違いしかありません。処理の流れはNotifyAllと同じです。

== Monitorクラス

HotspotVMには@<code>{Monitor}というクラスが実装されており、VM内部で利用するスレッドはこのクラスを利用して排他制御します。

@<code>{Monitor}クラスには次のようにメンバ関数が定義されています。
//source[share/vm/runtime/mutex.hpp]{
87: class Monitor : public CHeapObj {

177:  public:
185:   bool wait(bool no_safepoint_check = !_no_safepoint_check_flag,
186:             long timeout = 0,
187:             bool as_suspend_equivalent = !_as_suspend_equivalent_flag);
188:   bool notify();
189:   bool notify_all();

193:   void lock(Thread *thread);
194:   void unlock();
//}

この@<code>{Monitor}クラスの1インスタンスが例として取り上げたレンタルショップのモニタに相当します。
たとえば、10個@<code>{Monitor}のインスタンスを作れば、10個のレンタルショップのモニタが作られたことになります。
そして、それぞれの店はそれぞれの共有リソースを管理します。
また、客（スレッド）はモニタのしきたりに則って、どの店にも入店することができます。

実際にコードを見てみないとイメージをつかめないと思いますので、次に@<code>{Monitor}クラスのサンプルコードを示しました。

//emlistnum{
// new Monitor(Mutex::safepoint, "Test Monitor"); で初期化される
Monitor* shop_monitor;
// レンタルショップを表すグローバル変数
RentalShop* rental_shop;

class Client {
  Board* _snowboard;

  // ...
}
//}

2行目には@<code>{Monitor}インスタンスへのポインタを保持するグローバル変数の@<code>{shop_monitor}を定義します。
4行目にはレンタルショップを表す@<code>{rental_shop}グローバル変数を定義します。
これらのグローバル変数はどこか別の関数で初期化されると想定します。

6行目に@<code>{Client}クラスを定義します。
@<code>{Client}クラスはレンタルショップを訪問する客を表現しており、@<code>{shop_monitor}はレンタルショップのモニタを表現しています。
@<code>{Client}は@<code>{_snowboard}メンバ変数を持っており、ここに借りたボードを格納します。

次に、ボードをレンタルするメンバ関数を@<code>{Client}クラスの@<code>{rent()}として定義します。

//emlistnum{
  void rent() {
    // ロック
    shop_monitor.lock();
    // ボードがある状態になるまで待つ
    while (rental_shop.snowboards.empty()) {
      shop_monitor.wait();
    }
    // 借りる
    _snowboard = rental_shop.snowboards.pop();
    shop_monitor.unlock();
  }
//}

3行目で@<code>{shop_monitor.lock()}を呼び出し、モニタのロックを取得します。
すでにモニタがロックされていた場合は、ロックが取得できるまで待つことになります。
5,6行目でボードが空になるまでモニタをアンロックして待ちます。
別の客に起こされたら、9行目でボードを取り出し、@<code>{_snowboard}に格納します。
そして、10行目でモニタをアンロックします。

次に、ボードを返却するメンバ関数を@<code>{return()}として定義します。

//emlistnum{
  void return() {
    shop_monitor.lock();
    // ボードを返す
    rental_shop.snowboards.push(_snowboard);
    _snowboard = NULL;
    // 待っているスレッドを1つだけ呼び出す
    shop_monitor.notify();
    shop_monitor.unlock();
  }
//}

こちらも@<code>{rent()}と同じくロックを取得した後で、ボードを返却します。
返却したら7行目で待っている客を1人だけ呼び出し、8行目でモニタをアンロックします。
呼び出された客（@<code>{rent()}で待っていた客）はモニタをロックしてボードを借ります。

== Mutexクラス

ミューテックスを表現する@<code>{Mutex}クラスもHotspotVMには実装されています。
@<code>{Mutex}クラスは@<code>{Monitor}クラスを継承して作られ、@<code>{Monitor}クラスの機能をほぼそのまま使います。

//source[share/vm/runtime/mutex.hpp]{
262: class Mutex : public Monitor {
263:  public:
264:    Mutex (int rank, const char *name, bool allow_vm_block=false);
265:    ~Mutex () ;
266:  private:
267:    bool notify ()    { ShouldNotReachHere(); return false; }
268:    bool notify_all() { ShouldNotReachHere(); return false; }
269:    bool wait (bool no_safepoint_check,
                   long timeout,
                   bool as_suspend_equivalent) {
270:      ShouldNotReachHere() ;
271:      return false ;
272:    }
273: };
//}

ミューテックスはロックとアンロックだけあればよいので、267〜272行目で@<code>{notify()},@<code>{notify_all()},@<code>{wait()}を呼び出せないように再定義しています。

== MutexLockerクラス

@<code>{MutexLocker}クラスはロックの範囲をわかりやすく定義するのに役立つクラスです。

//source[share/vm/runtime/mutexLocker.hpp]{
156: class MutexLocker: StackObj {
157:  private:
158:   Monitor * _mutex;
159:  public:
160:   MutexLocker(Monitor * mutex) {
163:     _mutex = mutex;
164:     _mutex->lock();
165:   }

175:   ~MutexLocker() {
176:     _mutex->unlock();
177:   }
178: 
179: };
//}

このクラスがおこなうことはコンストラクタでメンバ変数の@<code>{_mutex}をロックし、デストラクタで@<code>{_mutex}をアンロックすることだけです。

定義自体はシンプルですが、@<code>{MutexLocker}クラスを利用すれば、@<hd>{Monitorクラス}で紹介した@<code>{rent()}関数は次のように記述できます。

//emlistnum{
  void rent() {
    {
      MonitorLoker locker(shop_monitor);
      while (rental_shop.snowboards.empty()) {
        shop_monitor.wait();
      }
      _snowboard = rental_shop.snowboards.pop();
    }
  }
//}

3行目で@<code>{MonitorLoker}インスタンスを生成する際に、コンストラクタによって@<code>{shop_monitor}がロックされます。
8行目ではスタックに割り当てられた@<code>{MonitorLoker}インスタンスが解放されますので、デクストラクタによって@<code>{shop_monitor}がアンロックされます。

上記のように、@<code>{MutexLocker}クラスを利用するとロックが必要な処理の範囲がわかりやすくなります。
また、大域脱出の際（例外発生時など）にモニタのアンロックをし忘れる、といった凡ミスも防ぐことができます。
そのため、HotspotVM内のコードではこの@<code>{MutexLocker}クラスや、Nullチェックの拡張を加えた@<code>{MutexLockerEx}クラスが多用されています。

//pagebreak

===[column]アニメキャラのGC的分類

//indepimage[anime_with_gc][][scale=5]

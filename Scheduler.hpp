てすｔ

/**
 * @file Scheduler.hpp
 * @date 2020/04/08
 * @author kanazawama
 */

#pragma once
#include <functional>
#include "Grobal.hpp"

#include "../../../src/watch_framework/core/SchedulerConfig.hpp"

/**
 * イベントの優先度。高い優先度のイベントほど優先的にディスパッチされる。
 */
enum EventPriority : uint8_t {
	HIGHEST = 0,
	MIDHIGH = 1,
	MIDLOW = 2,
	LOWEST = 3
};

/**
 * イベントディスパッチ時にイベントハンドラに渡される引数。
 * イベントの送信側から受信側へ、この64bitのデータをスケジューラ経由で受け渡す。
 * 送受信間で予め取り決めた形式の構造体を共用体として、この64bitにパックして渡す。
 * @attention 大きなデータを渡す必要がある場合は64bit内にデータのポインタを含めて参照渡しする事になるが、スケジューラはRAMの解放処理には関与しない。
 */
typedef uint64_t EventArg;

/** 特にイベントハンドラに渡す値がない場合のダミー値。 */
static constexpr EventArg EVENT_ARG_BRANK = 0;

/**
 * イベントハンドラのポインタ。イベントディスパッチ時にこの参照先のメソッドがスケジューラーから実行される。
 * @attention 非staticなメソッドのポインタを格納する場合には実行されるまでに参照が切れないように注意。
 * @param eventArg
 */
typedef void (*P_EventHandler)(EventArg eventArg);

/**
 * イベントキューの内部インデックスの型。符号なし8bit、最大255個。
 */
typedef uint8_t EventQueIndex;


/**
 * @brief 登録されたイベント（関数ポインタ）を非同期で優先度付きディスパッチするメインスケジューラー。
 * @brief イベントドリブンなメソッド単位でのノンプリエンティブマルチタスクを実現する。
 * @brief 複数のイベントが登録されている場合は、優先度が高い順に実行されることになる。
 * @brief イベントは64bit引数付きの関数ポインタの形式でキューイングされる。
 */
class Scheduler
{
	private:
		Scheduler();//static

	public:

		/**
		 * @brief Static変数の初期化処理。 Initializer から呼ばれる。
		 */
		static void initializeStatic(void);

		/**
		 * @brief イベントディスパッチループを実行開始する。戻ってこない。初期のイベントを登録後に開始する事。
		 */
		static void startDispatchEventLoop(void);

		/**
		 * @brief 指定優先度のイベントを登録する。
		 * @param eventPriority イベントの実行優先度
		 * @param p_EventHandler イベントの実行ハンドラ
		 * @param eventArg 実行時にハンドラに渡す引数（64bit）
		 */
		static void addEvent(EventPriority eventPriority,P_EventHandler p_EventHandler,EventArg eventArg);

		/**
		 * @brief 全ての優先度のイベントキュー内を調べて、該当するイベントがあれば削除する。
		 * @brief 割り込み禁止中に呼び出すことで、イベントの同期キャンセルができる。
		 * @param p_EventHandler 呼び出し先がこのハンドラとマッチするものが削除される。
		 */
		static void deleteEvent(P_EventHandler p_EventHandler);

		/**
		 * @brief 指定優先度のイベントキュー内を調べて、該当するイベントがあれば削除する。
		 * @brief 割り込み禁止中に呼び出すことで、イベントの同期キャンセルができる。
		 * @brief 優先度が分かっていれば多少実行速度が速い。
		 * @param eventPriority イベント優先度
		 * @param p_EventHandler 呼び出し先がこのハンドラとマッチするものが削除される。
		 * @sa addEvent
		 */
		static void deleteEvent(EventPriority eventPriority,P_EventHandler p_EventHandler);

		/**
		 * @brief イベントキューの空き容量を返す。
		 * @return
		 * @note キューの空きが少ない場合にスイッチイベントなどの新規追加を諦めて捨てる等の処理用。
		 */
		static EventQueIndex getEventFreeCapacity(void);

		/**
		 * @brief 現在登録されているイベント数を返す。
		 * @return
		 * @note スリープへの移行判定処理用。
		 */
		static EventQueIndex getEventCount(void);

	private:

		/** キュー本体：イベントディスパッチ時の呼び出し先を格納。 */
		static P_EventHandler que_handler[];

		/** キュー本体：イベントディスパッチ時の引数を格納。 */
		static EventArg que_arg[];

		/** キュー本体：ひとつ後の要素のインデックスを格納。末尾の要素の場合は無効値。 */
		static EventQueIndex que_nextIndex[];

		/** キュー本体：ひとつ前の要素のインデックスを格納。先頭の要素の場合は無効値。 */
		static EventQueIndex que_backIndex[];

		/** ブランクと各優先度の仮想キューの情報 */
		typedef struct _EventQueue
		{
			EventQueIndex count; 			//!< 格納されている要素数。
			EventQueIndex firstIndex; 	//!< 先頭要素のインデックス。countが0の場合は無効値。
			EventQueIndex lastIndex; 		//!< 末尾要素のインデックス。countが0の場合は無効値。
		} EventQueue;

		/** ブランクキュー */
		static EventQueue brankQue;

		/** 優先度別のイベントキュー */
		static EventQueue eventQue[];

	private:

		/**
		 * キューの先頭の要素を取り出す。
		 * @param p_eventQueue
		 * @return 取り出した要素のインデックス。（並び順ではなく配列の絶対位置）
		 * @note 取り出した要素は別途ブランクに戻すこと。
		 */
		static EventQueIndex dequeueFromHead(EventQueue* p_eventQueue);

		/**
		 * キューの末尾の要素を取り出す。
		 * @param p_eventQueue
		 * @return 取り出した要素のインデックス。（並び順ではなく配列の絶対位置）
		 * @note 取り出した要素は別途ブランクに戻すこと。
		 */
		static EventQueIndex dequeueFromTail(EventQueue* p_eventQueue);

		/**
		 * キューの指定したインデックスの要素を取り出す。
		 * @param p_eventQueue
		 * @param targetIndex 取り出す要素のインデックス（並び順ではなく配列の絶対位置）
		 * @return 取り出した要素のインデックス（＝ targetIndex ）、（並び順ではなく配列の絶対位置）
		 * @note 取り出した要素は別途ブランクに戻すこと。
		 */
		static EventQueIndex dequeueFromAbsoluteIndex(EventQueue* p_eventQueue,int8_t targetIndex);

		/**
		 * キューの先頭に要素を追加する。
		 * @param p_eventQueue
		 * @param brankIndex 要素の格納先のインデックス。（並び順ではなく配列の絶対位置）
		 * @param p_EventHandler
		 * @param eventArg
		 */
		static void enqueueToHead(EventQueue* p_eventQueue,EventQueIndex brankIndex,P_EventHandler p_EventHandler,EventArg eventArg);

		/**
		 * キューの末尾に要素を追加する。
		 * @param p_eventQueue
		 * @param brankIndex 要素の格納先のインデックス。（並び順ではなく配列の絶対位置）
		 * @param p_EventHandler
		 * @param eventArg
		 */
		static void enqueueToTail(EventQueue* p_eventQueue,EventQueIndex brankIndex,P_EventHandler p_EventHandler,EventArg eventArg);

		/**
		 * キューから呼び出し先が一致するイベントを探して削除する。
		 * @param p_eventQueue
		 * @param p_EventHandler
		 */
		static void deleteEvent(EventQueue* p_eventQueue,P_EventHandler p_EventHandler);

		/**
		 * ブランクキューの先頭に要素を追加する。
		 * @param p_eventQueue
		 * @param brankIndex 要素の格納先のインデックス。（並び順ではなく配列の絶対位置）
		 */
		static void enqueueToHeadOfBrankQueue(EventQueIndex brankIndex);
};


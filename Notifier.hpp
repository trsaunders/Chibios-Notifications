#ifndef __NOTIFIER_HPP__
#define __NOTIFIER_HPP__

#include "ch.h"
#include "hal.h"

template <class MsgType> class NotifierMsg;
template <class MsgType, int N> class Listener;
template <class MsgType> class NotifierItem;
template <class MsgType> class Notifier;

/* data message holder */
template <class MsgType> class NotifierMsg {
protected:
	uint16_t ref_count;
public:
	bool_t send(Mailbox *, MsgType *);
	bool_t dereference(void);
	void reset(void);
} __attribute__((aligned(sizeof(stkalign_t))));

template <class MsgType, int N> class Listener {
private:
	NotifierItem<MsgType> notifier;
	uint8_t buffer[N*MEM_ALIGN_NEXT(sizeof(MsgType))] __attribute__((aligned(sizeof(stkalign_t))));
	MsgType *mail[N];
	Mailbox mailbox;
	Notifier<MsgType> *source;
public:
	int getSize(void);
	Listener(Notifier<MsgType> *);
	MsgType *get(void);
	void release(MsgType *);
	size_t available(void);
};

template <class MsgType> class Notifier {
private:
	NotifierItem<MsgType> *notify;
	MemoryPool pool;
public:
	int broadcast(MsgType *);
	int listeners(void);
	void listen(NotifierItem<MsgType> *notifier);
	void release(MsgType*);
	void add(void*, size_t N);
	MsgType *alloc(void);
	Notifier(void);
};

template <class MsgType> class NotifierItem {
private:
	Notifier<MsgType> * const source;
	Mailbox * const mailbox;
	NotifierItem<MsgType> *next;
public:
	NotifierItem(Notifier<MsgType> *, Mailbox *);
	void link(NotifierItem<MsgType> *);
	NotifierItem<MsgType> *last(void);
	NotifierItem<MsgType> *notify(MsgType *);
	NotifierItem<MsgType> *notify(MsgType *, int &);
	MsgType *get(void);
	void release(MsgType *);
	size_t available(void);
};

/* called from within system lock */
template <class MsgType> bool_t NotifierMsg<MsgType>::send(Mailbox *mailbox, MsgType *msg) {
	/* post message to mailbox */
	if(chMBPostS(mailbox, (msg_t)msg, TIME_IMMEDIATE) == RDY_OK) {
		ref_count++;
		return TRUE;
	}
	return FALSE;
}

/* called from within system lock */
template <class MsgType> bool_t NotifierMsg<MsgType>::dereference(void) {
	return (ref_count-- <= 1) ? true : false;
}

template <class MsgType> void NotifierMsg<MsgType>::reset(void) {
	ref_count = 0;
}

/*********************************
* NotifierItem
* Not combined with Listener because Listeners can have different length N
**********************************/
template <class MsgType> NotifierItem<MsgType>::NotifierItem(Notifier<MsgType> *src, Mailbox *mail) :
	source(src), mailbox(mail){
	next = NULL;
}

template <class MsgType> NotifierItem<MsgType> *NotifierItem<MsgType>::last(void) {
	NotifierItem<MsgType> *notify_next = this;
	while(notify_next->next != NULL)
		notify_next = notify_next->next;
	return notify_next;
}

template <class MsgType> void NotifierItem<MsgType>::link(NotifierItem<MsgType> *link) {
	next = link;
}

/* called from within lock */
template <class MsgType> NotifierItem<MsgType> *NotifierItem<MsgType>::notify(MsgType *msg, int &n) {
	/* allocate data from memory pool */
	if(msg->send(mailbox, msg))
		n++;
	return next;
}

template <class MsgType> NotifierItem<MsgType> *NotifierItem<MsgType>::notify(MsgType *msg) {
	int n;
	return this->notify(msg, n);
}

template <class MsgType> MsgType *NotifierItem<MsgType>::get(void) {
	MsgType *t;
	chMBFetch(mailbox, (msg_t *)&t, TIME_INFINITE);
	return t;
}

template <class MsgType> void NotifierItem<MsgType>::release(MsgType *d) {
	source->release(d);
}

template <class MsgType> size_t NotifierItem<MsgType>::available(void) {
	size_t count;
	chSysLock();
	count = chMBGetUsedCountI(mailbox);
	chSysUnlock();
	return count;
}

/*********************************
* Notifier 
**********************************/

template <class MsgType> Notifier<MsgType>::Notifier(void) {
	notify = NULL;
	chPoolInit(&pool, sizeof(MsgType), NULL);
}

template <class MsgType> int Notifier<MsgType>::listeners(void) {
	return 1;
}

template <class MsgType> void Notifier<MsgType>::listen(NotifierItem<MsgType> *notifier) {
	chSysLock();
	if(this->notify == NULL) {
		/* we have no listeners */
		this->notify = notifier;
	} else {
		/* link notifier to the last listener in list */
		notify->last()->link(notifier);
	}
	chSysUnlock();
}

template <class MsgType> int Notifier<MsgType>::broadcast(MsgType *msg) {
	NotifierItem<MsgType> *next = notify;
	if(next == NULL)
		return 0;
	
	int n=0;
	chSysLock();
	while(next != NULL)
		next = next->notify(msg, n);
	chSysUnlock();
	
	return n;
}

template <class MsgType> void Notifier<MsgType>::release(MsgType* msg) {
	chSysLock();
	/* if the reference count is reduced to zero then free the memory */
	if(msg->dereference())
		chPoolAddI(&pool, msg);
	chSysUnlock();
}

template <class MsgType> void Notifier<MsgType>::add(void* buffer, size_t N) {
	chPoolLoadArray(&pool, buffer, N);
}

template <class MsgType> MsgType *Notifier<MsgType>::alloc(void) {
	MsgType *msg = (MsgType *)chPoolAlloc(&pool);
	/* reset reference count */
	if(msg != NULL)
		msg->reset();

	return msg;
}

/*********************************
* Listener
**********************************/

template <class MsgType, int N> int Listener<MsgType,N>::getSize(void) {
	return N;
}

template <class MsgType, int N> Listener<MsgType,N>::Listener(Notifier<MsgType> *src)
: notifier(src, &mailbox), source(src) {
	chMBInit(&mailbox, (msg_t *)mail, N);

	/* add all items to pool */
	src->add(buffer, N);

	/* register as a listener on the source */
	src->listen(&notifier);
}

template <class MsgType, int N> MsgType *Listener<MsgType,N>::get(void) {
	return notifier.get();
}

template <class MsgType, int N> void Listener<MsgType,N>::release(MsgType *d) {
	notifier.release(d);
}

template <class MsgType, int N> size_t Listener<MsgType,N>::available(void) {
	return notifier.available();
}

#endif /* __NOTIFIER_HPP__ */

// jitterring packets
#include <stdlib.h>
#include "iup.h"
#include "common.h"
#define NAME "jitter"
#define JITTER_MIN "0"
#define JITTER_MAX "15000"
#define KEEP_AT_MOST 2000
// send FLUSH_WHEN_FULL packets when buffer is full
#define FLUSH_WHEN_FULL 800
#define JITTER_DEFAULT 50

// don't need a chance
static Ihandle *inboundCheckbox, *outboundCheckbox, *timeInput;

static volatile short jitterEnabled = 0,
                      jitterInbound = 1,
                      jitterOutbound = 1,
                      jitterTime = JITTER_DEFAULT; // default for 50ms

static PacketNode jitterHeadNode = {0}, jitterTailNode = {0};
static PacketNode *bufHead = &jitterHeadNode, *bufTail = &jitterTailNode;
static int bufSize = 0;

static INLINE_FUNCTION short isBufEmpty() {
    short ret = bufHead->next == bufTail;
    if (ret) assert(bufSize == 0);
    return ret;
}

static Ihandle *jitterSetupUI() {
    Ihandle *jitterControlsBox = IupHbox(
        inboundCheckbox = IupToggle("Inbound", NULL),
        outboundCheckbox = IupToggle("Outbound", NULL),
        IupLabel("Jitter(ms):"),
        timeInput = IupText(NULL),
        NULL
        );

    IupSetAttribute(timeInput, "VISIBLECOLUMNS", "4");
    IupSetAttribute(timeInput, "VALUE", STR(JITTER_DEFAULT));
    IupSetCallback(timeInput, "VALUECHANGED_CB", uiSyncInteger);
    IupSetAttribute(timeInput, SYNCED_VALUE, (char*)&jitterTime);
    IupSetAttribute(timeInput, INTEGER_MAX, JITTER_MAX);
    IupSetAttribute(timeInput, INTEGER_MIN, JITTER_MIN);
    IupSetCallback(inboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(inboundCheckbox, SYNCED_VALUE, (char*)&jitterInbound);
    IupSetCallback(outboundCheckbox, "ACTION", (Icallback)uiSyncToggle);
    IupSetAttribute(outboundCheckbox, SYNCED_VALUE, (char*)&jitterOutbound);

    // enable by default to avoid confusing
    IupSetAttribute(inboundCheckbox, "VALUE", "ON");
    IupSetAttribute(outboundCheckbox, "VALUE", "ON");

    if (parameterized) {
        setFromParameter(inboundCheckbox, "VALUE", NAME"-inbound");
        setFromParameter(outboundCheckbox, "VALUE", NAME"-outbound");
        setFromParameter(timeInput, "VALUE", NAME"-time");
    }

    return jitterControlsBox;
}

static void jitterStartUp() {
    if (bufHead->next == NULL && bufTail->next == NULL) {
        bufHead->next = bufTail;
        bufTail->prev = bufHead;
        bufSize = 0;
    } else {
        assert(isBufEmpty());
    }
    startTimePeriod();
}

static void jitterCloseDown(PacketNode *head, PacketNode *tail) {
    PacketNode *oldLast = tail->prev;
    UNREFERENCED_PARAMETER(head);
    // flush all buffered packets
    LOG("Closing down jitter, flushing %d packets", bufSize);
    while(!isBufEmpty()) {
        insertAfter(popNode(bufTail->prev), oldLast);
        --bufSize;
    }
    endTimePeriod();
}

static short jitterProcess(PacketNode *head, PacketNode *tail) {
    
    PacketNode *pac = tail->prev;
    short ret = FALSE;
    // pick up all packets and fill in the current time
    while (bufSize < KEEP_AT_MOST && pac != head) {
        if (checkDirection(pac->addr.Outbound, jitterInbound, jitterOutbound)) {
            static BOOL needLatency = FALSE;
            if (needLatency){
                insertAfter(popNode(pac), bufHead)->timestamp = timeGetTime() + (rand() % (2 * jitterTime));
            }
            else{
                insertAfter(popNode(pac), bufHead)->timestamp = timeGetTime();
            }
            needLatency = !needLatency;
            ++bufSize;
            pac = tail->prev;
        } else {
            pac = pac->prev;
        }
    }

    // try sending overdue packets from buffer tail
    DWORD currentTime = timeGetTime();
    while (!isBufEmpty()) {
        pac = bufTail->prev;
        if (currentTime >= pac->timestamp){
            insertAfter(popNode(pac), head); // sending queue is already empty by now
            --bufSize;
            LOG("Send jitterged packets.");
            ret = TRUE;
        }
        else{
            LOG("Sent some jitterged packets, still have %d in buf", bufSize);
            continue;
        }
    }

    // if buffer is full just flush things out
    if (bufSize >= KEEP_AT_MOST) {
        int flushCnt = FLUSH_WHEN_FULL;
        while (flushCnt-- > 0) {
            insertAfter(popNode(bufTail->prev), head);
            --bufSize;
        }
    }

    return ret;
    
}

Module jitterModule = {
    "Jitter",
    NAME,
    (short*)&jitterEnabled,
    jitterSetupUI,
    jitterStartUp,
    jitterCloseDown,
    jitterProcess,
    // runtime fields
    0, 0, NULL
};
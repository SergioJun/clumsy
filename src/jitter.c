// jitterring packets
#include <stdlib.h>
#include "iup.h"
#include "common.h"
#define NAME "jitter"
#define JITTER_MIN "0"
#define JITTER_MAX "15000"
#define JITTER_DEFAULT 10

// don't need a chance
static Ihandle *inboundCheckbox, *outboundCheckbox, *timeInput;

static volatile short jitterEnabled = 0,
                      jitterInbound = 1,
                      jitterOutbound = 1,
                      jitterTime = JITTER_DEFAULT; // default for 50ms

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

static void jitterStartUp(){
    LOG("jitter enabled");
}

static void jitterCloseDown(PacketNode *head, PacketNode *tail){
    UNREFERENCED_PARAMETER(head);
    UNREFERENCED_PARAMETER(tail);
    LOG("jitter disabled");
}

static short jitterProcess(PacketNode *head, PacketNode *tail) {
    PacketNode *pac = tail->prev;
    short ret = FALSE;
    // pick up all packets and fill in lagDiff info
    while (pac != head) {
        if (checkDirection(pac->addr.Outbound, jitterInbound, jitterOutbound)){
            pac->lagDiff = (rand() % (2 * jitterTime)) - jitterTime; //[-jitterTime, jitterTime-1]
            ret = TRUE;
        }
        pac = pac->prev;
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
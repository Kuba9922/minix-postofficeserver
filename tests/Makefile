INC=-Iinc
CFLAGS=$(INC)
TBIN=tbin
TSRC=tsrc
FX= test_basic.x test_bomb.x test_forward.x test_retrieve_wait.x test_send_back.x test_signals.x
FFX= ${FX:S/^/$(TBIN)\//}

all: $(FFX)

$(TBIN)/test_basic.x: $(TSRC)/test_basic.c
	cc $(CFLAGS) $(.ALLSRC) -o $(.TARGET)

$(TBIN)/test_bomb.x: $(TSRC)/test_bomb.c
	cc $(CFLAGS) $(.ALLSRC) -o $(.TARGET)

$(TBIN)/test_forward.x: $(TSRC)/test_forward.c
	cc $(CFLAGS) $(.ALLSRC) -o $(.TARGET)

$(TBIN)/test_retrieve_wait.x: $(TSRC)/test_retrieve_wait.c
	cc $(CFLAGS) $(.ALLSRC) -o $(.TARGET)

$(TBIN)/test_send_back.x: $(TSRC)/test_send_back.c
	cc $(CFLAGS) $(.ALLSRC) -o $(.TARGET)

$(TBIN)/test_signals.x: $(TSRC)/test_signals.c
	cc $(CFLAGS) $(.ALLSRC) -o $(.TARGET)

clean:
	rm $(TBIN)/*.x

topic "PtyWaitEvent";
[i448;a25;kKO9;2 $$1,0#37138531426314131252341829483380:class]
[l288;2 $$2,2#27521748481378242620020725143825:desc]
[0 $$3,0#96390100711032703541132217272105:end]
[H6;0 $$4,0#05600065144404261032431302351956:begin]
[i448;a25;kKO9;2 $$5,0#37138531426314131252341829483370:item]
[l288;a4;*@5;1 $$6,6#70004532496200323422659154056402:requirement]
[l288;i1121;b17;O9;~~~.1408;2 $$7,0#10431211400427159095818037425705:param]
[i448;b42;O9;2 $$8,8#61672508125594000341940100500538:tparam]
[b42;2 $$9,9#13035079074754324216151401829390:normal]
[2 $$0,0#00000000000000000000000000000000:Default]
[{_} 
[ {{10000@(113.42.0) [s0;%% [*@7;4 PtyWaitEvent]]}}&]
[s0; &]
[s1;:Upp`:`:PtyWaitEvent: [@(0.0.255)3 class][3 _][*3 PtyWaitEvent]&]
[s2;%% This class provides a mechanism for waiting on multiple pseudoterminal 
(pty) processes to become ready for I/O operations. On windows 
it uses I/O Completion Ports (IOCP) to wait for events on process 
and pipe handles. On POSIX`-compliant operating systems it uses 
the poll() system call to wait for events on pty/file descriptors.&]
[s0;i448;a25;kKO9;:noref:@(0.0.255) &]
[ {{10000F(128)G(128)@1 [s0;%% [* Public Method List]]}}&]
[s3; &]
[s5;:Upp`:`:PtyWaitEvent`:`:Clear`(`): [@(0.0.255) void] [* Clear]()&]
[s2;%% Removes all processes from the wait set.&]
[s3; &]
[s4; &]
[s5;:Upp`:`:PtyWaitEvent`:`:Add`(const APtyProcess`&`,dword`): [@(0.0.255) void] 
[* Add]([@(0.0.255) const] APtyProcess[@(0.0.255) `&] [*@3 pty], dword 
[*@3 events])&]
[s2;%% Adds a [%-*@3 pty] process to the wait set with specified events 
to monitor. Events can be WAIT`_READ, WAIT`_WRITE or both.&]
[s3; &]
[s4; &]
[s5;:Upp`:`:PtyWaitEvent`:`:Wait`(int`): [@(0.0.255) bool] [* Wait]([@(0.0.255) int] 
[*@3 timeout])&]
[s2;%% Waits for any of the monitored processes to become ready for 
the specified events.  Returns true if at least one process has 
a pending event, false if timeout occurred with no events.&]
[s3; &]
[s4; &]
[s5;:Upp`:`:PtyWaitEvent`:`:Get`(int`)const: [@8 dword ][* Get]([@(0.0.255) int] 
[*@3 i]) [@(0.0.255) const]&]
[s5;:Upp`:`:PtyWaitEvent`:`:operator`[`]`(int`)const: [@8 dword] operator[@(0.0.255) `[`]
]([@(0.0.255) int] [*@3 i]) [@(0.0.255) const]&]
[s0;l288;%% Returns events that triggered for pty process at index 
[%-*@3 i] (indicies are specified by order of Add calls) as binary 
or of WAIT`_READ, WAIT`_WRITE. If there were no events for requested 
pty, returns 0.&]
[s3; &]
[ {{10000F(128)G(128)@1 [s0;%% [* Constructor detail]]}}&]
[s3; &]
[s5;:Upp`:`:PtyWaitEvent`:`:PtyWaitEvent`(`): [* PtyWaitEvent]()&]
[s2;%% Default constructor.&]
[s3; ]]
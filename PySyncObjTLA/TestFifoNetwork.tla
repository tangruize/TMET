-------------------------- MODULE TestFifoNetwork --------------------------

EXTENDS Naturals, Sequences

VARIABLE pc, counter

_nodes == { "s0", "s1", "s2" }
_test_m(data) == [ data |-> data, seq |-> (data + 3) ]
_test_msgs ==
    [ s0 |-> [ s1 |-> <<_test_m(1), _test_m(2)>>,
               s2 |-> <<_test_m(3), _test_m(4), _test_m(5)>> ],
      s1 |-> [ s0 |-> <<_test_m(6), _test_m(7), _test_m(8), _test_m(9), _test_m(10)>>,
               s2 |-> <<_test_m(11), _test_m(12)>> ],
      s2 |-> [ s0 |-> <<_test_m(13), _test_m(14)>>,
               s1 |-> <<_test_m(15), _test_m(16), _test_m(17)>> ] ]

netman1 == [ n_sent |-> 20, n_recv |-> 3, n_wire |-> 17, conn |-> <<_nodes>> ]
netman2 == [ n_sent |-> 20, n_recv |-> 3, n_wire |-> 17, conn |-> <<{"s1", "s2"}>> ]
msgs == _test_msgs

net1 == INSTANCE FifoNetwork WITH NULL_MSG <- [ NULL_MSG |-> "" ],
    FLUSH_DISCONN <- TRUE, _msgs <- msgs, _netman <- netman1, _netcmd <- TRUE
net2 == INSTANCE FifoNetwork WITH NULL_MSG <- [ NULL_MSG |-> "" ],
    FLUSH_DISCONN <- TRUE, _msgs <- msgs, _netman <- netman2, _netcmd <- TRUE
INSTANCE Debug WITH PROMPT <- counter

---- \* Test common functions

_test_sec_common == <<"#### Section: common ####">>
_test_get_nodes == <<"_test_get_nodes", net1!_GetNodes, _nodes>>
_test_net_inc_helper == <<"_test_net_inc_helper", net1!_NetIncHelper("n_sent").n_sent, 21>>
_test_net_sum_wire == <<"_test_net_sum_wire", net1!NetSumWire.n_wire, netman1.n_wire>>

TESTCASES_COMMON ==
    <<_test_sec_common,
      _test_get_nodes,
      _test_net_inc_helper,
      _test_net_sum_wire>>

---- \* Test network partition

_test_sec_part == <<"#### Section: partition ####">>
_test_show_msgs ==
    <<"_test_show_msgs", msgs>>
_test_show_netman ==
    <<"_test_show_netman", netman1>>
_test_add_conn_s0_s1 ==
    <<"_test_add_conn_s0_s1", net1!NetAddConn({"s0", "s1"})[1]>>
_test_add_conn_s0_s1_net2 ==
    <<"_test_add_conn_s0_s1_net2", net2!NetAddConn({"s0", "s1"})[1]>>
_test_add_conn_s5 ==
    <<"_test_add_conn_s5", net1!NetAddConn({"s5"})[1]>>
_test_add_conn_s5_s6 ==
    <<"_test_add_conn_s5_s6", net1!NetAddConn({"s5", "s6"})[1]>>
_test_add_conn_s0_s1_s2_s3_s4_s5 ==
    <<"_test_add_conn_s0_s1_s2_s3_s4_s5", net1!NetAddConn({"s0", "s1", "s2", "s3", "s4", "s5"})[1]>>
_test_del_conn_s1 ==
    <<"_test_del_conn_s1", net1!NetDelConn({"s1"})>>
_test_del_conn_s1_net2 ==
    <<"_test_del_conn_s1_net2", net2!NetDelConn({"s1"})>>
_test_del_conn_s0_s1 ==
    <<"_test_del_conn_s0_s1", net1!NetDelConn({"s0", "s1"})>>
_test_part_conn_s1 ==
    <<"_test_part_conn_s1", net1!NetPartConn({"s1"})>>
_test_part_conn_s0_s1 ==
    <<"_test_part_conn_s0_s1", net1!NetPartConn({"s0", "s1"})>>
_test_flush_msgs ==
    LET ms == net1!NetPartConn({"s0"})[2]
    IN <<"_test_flush_msgs", ms>>
_test_flush_msgs_wire ==
    LET ms == _test_flush_msgs[2]
    IN <<"_test_flush_msgs_wire", net1!_WireSum(ms).n_wire, 5>>
_test_net_is_conn_s0_s1 ==
    <<"_test_net_is_conn_s0_s1", net1!NetIsConn("s0", "s1"), TRUE>>
_test_net_is_conn_s0_s1_net2 ==
    <<"_test_net_is_conn_s0_s1_net2", net2!NetIsConn("s0", "s1"), FALSE>>
_test_net_is_conn_s1_s9 ==
    <<"_test_net_is_conn_s1_s9", net1!NetIsConn("s1", "s9"), FALSE>>
_test_net_is_parted ==
    <<"_test_net_is_parted", net1!NetIsParted, FALSE>>
_test_net_is_parted_net2 ==
    <<"_test_net_is_parted_net2", net2!NetIsParted, TRUE>>

TESTCASES_PARTOTION ==
    <<_test_sec_part,
      _test_show_msgs,
      _test_show_netman,
      _test_add_conn_s0_s1,
      _test_add_conn_s0_s1_net2,
      _test_add_conn_s5,
      _test_add_conn_s5_s6,
      _test_add_conn_s0_s1_s2_s3_s4_s5,
      _test_del_conn_s1,
      _test_del_conn_s1_net2,
      _test_del_conn_s0_s1,
      _test_part_conn_s1,
      _test_part_conn_s0_s1,
      _test_flush_msgs,
      _test_flush_msgs_wire,
      _test_net_is_conn_s0_s1,
      _test_net_is_conn_s0_s1_net2,
      _test_net_is_conn_s1_s9,
      _test_net_is_parted,
      _test_net_is_parted_net2>>

---- \* Test network transport

_test_sec_trans == <<"#### Section: transport ####">>
_test_get_msg_s1_s2 ==
    <<"_test_get_msg_s1_s2", net1!NetGetMsg("s1", "s2")>>
_test_add_msg_s2_s0 ==
    <<"_test_add_msg_s2_s0", net1!NetAddMsgSrcDst("s2", "s0", [ data |-> "add" ])>>
_test_add_msg_s2_s0_net2 ==
    <<"_test_add_msg_s2_s0_net2", net2!NetAddMsgSrcDst("s2", "s0", [ data |-> "add_net2" ])>>
_test_del_msg_s2_s1 ==
    LET m == net1!NetGetMsg("s2", "s1")
        del == net1!NetDelMsg(m)
    IN <<"_test_del_msg_s2_s1", del>>
_test_reply_msg_s0_s2 ==
    LET request == net1!NetGetMsg("s2", "s0")
        response == [ data |-> "reponse" ]
    IN <<"_test_reply_msg_s0_s2", net1!NetReplyMsg(response, request)>>
_test_reply_msg_s0_s2_net2 ==
    LET request == net2!NetGetMsg("s2", "s0")
        response == [ data |-> "reponse" ]
    IN <<"_test_reply_msg_s0_s2_net2", net2!NetReplyMsg(response, request)>>
_test_batch_add_msgs ==
    LET m1 == [ src |-> "s0", dst |-> "s1", data |-> "m1" ]
        m2 == [ src |-> "s1", dst |-> "s2", data |-> "m2" ]
        m3 == [ src |-> "s2", dst |-> "s0", data |-> "m3" ]
        m4 == [ src |-> "s2", dst |-> "s1", data |-> "m4" ]
        ms == <<m1, m2, m3, m4>>
    IN <<"_test_batch_add_msgs", net1!NetBatchAddMsg(ms)>>
_test_batch_add_msgs_net2 ==
    LET m1 == [ src |-> "s0", dst |-> "s1", data |-> "m1" ]
        m2 == [ src |-> "s1", dst |-> "s2", data |-> "m2" ]
        m3 == [ src |-> "s2", dst |-> "s0", data |-> "m3" ]
        m4 == [ src |-> "s2", dst |-> "s1", data |-> "m4" ]
        ms == <<m1, m2, m3, m4>>
    IN <<"_test_batch_add_msgs_net2", net2!NetBatchAddMsg(ms)>>

TESTCASES_TRANSPORT ==
    <<_test_sec_trans,
      _test_get_msg_s1_s2,
      _test_add_msg_s2_s0,
      _test_add_msg_s2_s0_net2,
      _test_del_msg_s2_s1,
      _test_reply_msg_s0_s2,
      _test_reply_msg_s0_s2_net2,
      _test_batch_add_msgs,
      _test_batch_add_msgs_net2>>

---- \* All testcases

TESTCASES == <<TESTCASES_COMMON, TESTCASES_PARTOTION, TESTCASES_TRANSPORT>>

---- \* Init/Next

Init == pc = 1 /\ counter = 0

Next == IF pc <= Len(TESTCASES)
        THEN /\ IF counter + 1 <= Len(TESTCASES[pc])
                THEN pc' = pc /\ counter' = counter + 1
                ELSE pc' = pc + 1 /\ counter' = 1
             /\ IF pc' <= Len(TESTCASES)
                THEN PrintS(TESTCASES[pc'][counter'])
                ELSE UNCHANGED <<pc, counter>>
        ELSE UNCHANGED <<pc, counter>>

=============================================================================
\* Modification History
\* Last modified Sun Jul 10 11:38:02 CST 2022 by fedora
\* Last modified Thu May 05 21:41:19 CST 2022 by tangruize
\* Created Thu May 05 11:48:55 CST 2022 by tangruize

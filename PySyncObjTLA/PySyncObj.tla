----------------------------- MODULE PySyncObj -----------------------------

EXTENDS Sequences, Naturals, Integers, FiniteSets, TLC, Functions

(***************************************************************************
  Constants definitions
 ***************************************************************************)
CONSTANTS Servers                      \* Servers set
CONSTANTS Follower, Candidate, Leader  \* Server states
CONSTANTS Commands, NoOp               \* Commands set
CONSTANTS M_RV, M_RVR, M_AE, M_AER     \* Message types
CONSTANTS Parameters, Nil  \* Misc: state constraint parameters and placeholder

(***************************************************************************
  Variables definitions
 ***************************************************************************)
VARIABLES currentTerm, votedFor, log  \* Persistent variables
VARIABLES raftState, commitIndex      \* Volatile variables
VARIABLES nextIndex, matchIndex       \* Leader variables
VARIABLES votesCount                  \* Candidate variables

(***************************************************************************
  Network variables and instance
 ***************************************************************************)
VARIABLES netman, netcmd, msgs
INSTANCE FifoNetwork WITH FLUSH_DISCONN <- TRUE, NULL_MSG <- Nil,
    _msgs <- msgs, _netman <- netman, _netcmd <- netcmd

(***************************************************************************
  Type Ok
 ***************************************************************************)
TypeOkServerVars ==
    /\ currentTerm \in [ Servers -> Nat ]
    /\ votedFor    \in [ Servers -> Servers \cup {Nil} ]
    /\ raftState   \in [ Servers -> { Follower, Candidate, Leader } ]
TypeOkLeaderVars ==
    /\ nextIndex   \in [ Servers -> [ Servers -> Nat \ {0} ]]
    /\ matchIndex  \in [ Servers -> [ Servers -> Nat ]]
TypeOkCandidateVars ==
    /\ votesCount  \in [ Servers -> 0..Cardinality(Servers) ]
TypeOkLogVars ==
    /\ log \in [ Servers -> [ Nat -> 
        [ cmd: Commands \cup {NoOp}, idx: Nat \ {0}, term: Nat ]]]
    /\ commitIndex \in [ Servers -> Nat \ {0} ]
MsgType == [ seq: Nat, type: {M_RV, M_RVR, M_AE, M_AER}, data: Any ]
TypeOk ==
    /\ TypeOkServerVars
    /\ TypeOkLeaderVars
    /\ TypeOkCandidateVars
    /\ TypeOkLogVars
    /\ TypeOkFifoNetwork(Servers, MsgType)

(***************************************************************************
  Init variables
 ***************************************************************************)
InitServerVars ==
    /\ currentTerm = [ i \in Servers |-> 0 ]
    /\ votedFor    = [ i \in Servers |-> Nil ]
    /\ raftState   = [ i \in Servers |-> Follower ]
InitLeaderVars ==
\*    /\ nextIndex  = [ i \in Servers |-> [ j \in Servers \ {i} |-> 1 ]]
\*    /\ matchIndex = [ i \in Servers |-> [ j \in Servers \ {i} |-> 0 ]]
    /\ nextIndex  = [ i \in Servers |-> [ j \in Servers |-> 1 ]]
    /\ matchIndex = [ i \in Servers |-> [ j \in Servers |-> 0 ]]
InitCandidateVars ==
    \* Q: Directly count?
    /\ votesCount = [ i \in Servers |-> 0 ]
InitLogVars ==
    \* Q: Why No-Op added?
    /\ log = [ i \in Servers |-> <<[ cmd |-> NoOp, idx |-> 1, term |-> 0 ]>> ]
    \* Q: Init to 1?
    /\ commitIndex = [ i \in Servers |-> 1 ]
Init ==
    /\ InitServerVars
    /\ InitLeaderVars
    /\ InitCandidateVars
    /\ InitLogVars
    /\ InitFifoNetwork(Servers)

(***************************************************************************
  Helper functions
 ***************************************************************************)
\* Variable type sequences
serverVars    == <<currentTerm, votedFor, raftState>>
leaderVars    == <<nextIndex, matchIndex>>
candidateVars == <<votesCount>>
logVars       == <<log, commitIndex>>
netVars       == <<netman, netcmd, msgs>>
noNetVars     == <<serverVars, leaderVars, candidateVars, logVars>>
vars          == <<noNetVars, netVars>>

CheckStateIs(n,  s)   == raftState[n] = s
CheckStateIsNot(n, s) == raftState[n] /= s
SetState(n, s)        == raftState' = [ raftState EXCEPT ![n] = s ]

GetCurrentTerm(n)    == currentTerm[n]
GetCurrentLog(n)     == log[n][Len(log[n])]
GetCurrentLogTerm(n) == GetCurrentLog(n).idx
GetCurrentLogIdx(n)  == GetCurrentLog(n).term
AddCurrentTerm(n)    == currentTerm' = [ currentTerm EXCEPT ![n] = @+1 ]
SetCurrentTerm(n, t) == currentTerm' = [ currentTerm EXCEPT ![n] = t ]

GetVotedFor(n)       == votedFor[n]
SetVotedFor(n, v)    == votedFor' = [ votedFor EXCEPT ![n] = v ]
CheckNotVoted(n)     == GetVotedFor(n) = Nil
CheckTermNotVoted(n) ==
    IF currentTerm'[n] > GetCurrentTerm(n) THEN TRUE ELSE GetVotedFor(n) = Nil

CheckTermBecomeFollower(n, term) ==
    IF term > GetCurrentTerm(n)
    THEN /\ SetCurrentTerm(n, term)
         /\ SetState(n, Follower)
    ELSE UNCHANGED <<currentTerm, raftState>>

AddVotesCount(n) ==
    votesCount' = [ votesCount EXCEPT ![n] = @+1 ]
CheckVotesCountIsQuorum(n) ==
    votesCount'[n] * 2 > Cardinality(Servers)
ResetCandidateVotesCount(n) ==
    votesCount' = [ votesCount EXCEPT ![n] = 1 ]


(***************************************************************************
  Raft message handling
 ***************************************************************************)
HandleMsgRV(m) ==
    LET data == m.data
        src  == m.src
        dst  == m.dst
    IN /\ CheckTermBecomeFollower(dst, data.term)
       /\ IF /\ CheckStateIsNot(dst, Leader)     \* is not leader
             /\ data.term >= currentTerm'[dst]   \* term is smaller or equal
             /\ ~(\/ data.lastLogTerm < GetCurrentLogTerm(dst)  \* log is out dated
                  \/ /\ data.lastLogTerm = GetCurrentLogTerm(dst)
                     /\ data.lastLogIdx  < GetCurrentLogIdx(dst)
                  \/ ~CheckTermNotVoted(dst))    \* already voted
          THEN /\ SetVotedFor(dst, src)          \* vote
               /\ UNCHANGED <<leaderVars, candidateVars, logVars>>
               /\ LET reply == [ type |-> M_RVR, \* reply
                                 data |-> [ term |-> data.term ]]
                  IN NetUpdate2(NetReplyMsg(reply, m), <<"HandleMsgRV", "voted", dst, src>>)
           ELSE /\ UNCHANGED <<votedFor, leaderVars, candidateVars, logVars>>
                /\ NetUpdate2(NetDelMsg(m), <<"HandleMsgRV", "not-voted", dst, src>>) \* receive and dequeue msg

HandleMsgRVR(m) ==
    LET data == m.data
        src  == m.src
        dst  == m.dst
    IN /\ IF /\ CheckStateIs(dst, Candidate)
             /\ data.term = GetCurrentTerm(dst)
          THEN /\ AddVotesCount(dst)
               /\ IF CheckVotesCountIsQuorum(dst)
                  THEN /\ raftState'  = [ raftState EXCEPT ![dst] = Leader ]
                       /\ UNCHANGED <<leaderVars>>
                       \*/\ nextIndex'  = [ nextIndex EXCEPT ![dst] =
                       \*    [ j \in Servers \ {dst} |-> GetCurrentLogIdx(dst) + 1 ]]
                       \*/\ matchIndex' = [ matchIndex EXCEPT ![dst] =
                       \*    [ j \in Servers \ {dst} |-> 0 ]]
                       \* TODO send append entries
                       /\ NetUpdate2(NetDelMsg(m), <<"HandleMsgRVR", "become-leader", dst, src>>)
                  ELSE /\ UNCHANGED <<leaderVars>>
                       /\ NetUpdate2(NetDelMsg(m), <<"HandleMsgRVR", "not-quorum", dst, src>>)
          ELSE /\ UNCHANGED <<raftState, candidateVars, leaderVars>>
               /\ NetUpdate2(NetDelMsg(m), <<"HandleMsgRVR", "not-candidate-or-term-not-equal", dst, src>>)
       /\ UNCHANGED <<currentTerm, votedFor, logVars>>
\*       /\ NetUpdate2(NetDelMsg(m), <<"HandleMsgRVR", dst, src>>)

(***************************************************************************
  Election timeout and become candidate
 ***************************************************************************)
ElectionTimeout(n) ==
    /\ SetState(n, Candidate)
    /\ AddCurrentTerm(n)
    /\ ResetCandidateVotesCount(n)
    /\ SetVotedFor(n, n)
    /\ LET dsts == Servers \ {n}
           size == Cardinality(dsts)
           F[i \in 0..size] ==
                IF i = 0 THEN <<<<>>, dsts>>
                ELSE LET ms == F[i-1][1]
                         s == CHOOSE j \in F[i-1][2]: TRUE
                         m == [ src  |-> n, dst |-> s, type |-> M_RV,
                                data |-> [ term |-> currentTerm'[n],
                                           lastLogIdx  |-> GetCurrentLogIdx(n),
                                           lastLogTerm |-> GetCurrentLogTerm(n) ]]
                         remaining == F[i-1][2] \ {s}
                     IN <<Append(ms, m), remaining>>
       IN /\ NetUpdate2(NetBatchAddMsg(F[size][1]), <<"ElectionTimeout", n>>)
          /\ Assert(Cardinality(F[size][2]) = 0, <<"ElectionTimeout bug", F>>)
          /\ UNCHANGED <<leaderVars, logVars>>

(***************************************************************************
  Next actions
 ***************************************************************************)
DoHandleMsgRV ==
    \E src, dst \in Servers:
        /\ src /= dst
        /\ LET m == NetGetMsg(src, dst)
           IN /\ m /= Nil
              /\ m.type = M_RV
              /\ HandleMsgRV(m)

DoHandleMsgRVR ==
    \E src \in Servers, dst \in Servers:
        /\ src /= dst
        /\ LET m == NetGetMsg(src, dst)
           IN /\ m /= Nil
              /\ m.type = M_RVR
              /\ HandleMsgRVR(m)

DoElectionTimeout ==
    \E n \in Servers: ElectionTimeout(n)

DoNetworkPartition ==
    \E n \in Servers:
        /\ NetUpdate2(NetPartConn({n}), <<"DoNetworkPartition", n>>)
        /\ UNCHANGED noNetVars

DoNetworkCure ==
    /\ NetUpdate2(NetCureConn, <<"DoNetworkPartition">>)
    /\ UNCHANGED noNetVars

Next == \/ DoHandleMsgRV
        \/ DoHandleMsgRVR
        \/ DoElectionTimeout
        \/ DoNetworkPartition
        \/ DoNetworkCure

Spec == Init /\ [][Next]_vars

(***************************************************************************
  Invariants
 ***************************************************************************)
AtMostOneLeaderPerTerm ==
    LET TwoLeader ==
            \E i, j \in Servers:
                /\ i /= j
                /\ currentTerm[i] = currentTerm[j]
                /\ raftState[i] = Leader
                /\ raftState[j] = Leader
    IN ~TwoLeader


=============================================================================
\* Modification History
\* Last modified Sun Jul 17 15:11:12 CST 2022 by fedora
\* Created Wed Jul 06 13:50:24 CST 2022 by fedora

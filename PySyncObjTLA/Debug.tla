------------------------------- MODULE Debug -------------------------------

EXTENDS Naturals, Sequences, TLC

VARIABLE PROMPT

---- \* Debug functions

PrintV(name, value) ==
    LET prompt == IF PROMPT > 0 THEN <<PROMPT, name>> ELSE <<name>>
    IN Print(prompt, value) = value
PrintE(name, value, expect) ==
    LET info == IF value = expect THEN "PASS" ELSE "FAIL"
        prompt == IF PROMPT > 0 THEN <<PROMPT, name, info>> ELSE <<name, info>>
    IN Print(prompt, value) = value
PrintS(seq) ==
    IF Len(seq) = 1 THEN PrintT(seq[1])
    ELSE IF Len(seq) = 2 THEN PrintV(seq[1], seq[2])
         ELSE PrintE(seq[1], seq[2], seq[3])

=============================================================================
\* Modification History
\* Last modified Thu May 05 21:15:04 CST 2022 by tangruize
\* Created Sat Apr 23 16:39:55 CST 2022 by tangruize

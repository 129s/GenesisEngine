#

- Tilemap 融合方案：`docs/architecture/TILEMAP_INTEGRATION.md`


-
 
图
到
矩
阵
映
射
：
`
d
o
c
s
/
a
r
c
h
i
t
e
c
t
u
r
e
/
G
R
A
P
H
_
T
O
_
G
R
I
D
.
m
d
`



 



G



e



n



e



s



i



s



E



n



g



i



n



e



 



文



档



总



览













本



文



档



目



录



汇



总



当



前



可



用



的



技



术



文



档



、



路



线



图



与



开



发



指



引



，



便



于



快



速



定



位



信



息



与



协



作



。













#



#



 



目



录



结



构








-



 



架



构



（



设



计



、



原



则



、



系



统



划



分



）



：



`



d



o



c



s



/



a



r



c



h



i



t



e



c



t



u



r



e



/



`



（



总



览



：



`



d



o



c



s



/



a



r



c



h



i



t



e



c



t



u



r



e



/



R



E



A



D



M



E



.



m



d



`



）








-



 



路



线



图



（



权



威



版



、



里



程



碑



与



历



史



）



：



`



d



o



c



s



/



r



o



a



d



m



a



p



/



`



（



总



览



：



`



d



o



c



s



/



r



o



a



d



m



a



p



/



R



E



A



D



M



E



.



m



d



`



）








-



 



指



南



（



构



建



、



运



行



、



工



具



）



：



`



d



o



c



s



/



g



u



i



d



e



s



/



`



（



C



L



I



：



`



d



o



c



s



/



g



u



i



d



e



s



/



s



a



n



d



b



o



x



-



c



l



i



.



m



d



`



）








-



 



状



态



（



进



度



、



待



办



）



：



`



d



o



c



s



/



s



t



a



t



u



s



/



`



（



进



度



：



`



d



o



c



s



/



s



t



a



t



u



s



/



p



r



o



g



r



e



s



s



-



s



u



m



m



a



r



y



.



m



d



`



，



待



办



：



`



d



o



c



s



/



s



t



a



t



u



s



/



t



o



d



o



.



m



d



`



）













#



#



 



快



速



导



航








-



 



架



构



概



览



：



`



d



o



c



s



/



a



r



c



h



i



t



e



c



t



u



r



e



/



R



E



A



D



M



E



.



m



d



`








-



 



愿



景



与



设



计



原



则



：



`



d



o



c



s



/



a



r



c



h



i



t



e



c



t



u



r



e



/



V



I



S



I



O



N



.



m



d



`








-



 



世



界



生



成



设



计



：



`



d



o



c



s



/



a



r



c



h



i



t



e



c



t



u



r



e



/



W



O



R



L



D



_



G



E



N



E



R



A



T



I



O



N



.



m



d



`








-



 



运



行



时



与



沙



盒



 



C



L



I



：



`



d



o



c



s



/



g



u



i



d



e



s



/



s



a



n



d



b



o



x



-



c



l



i



.



m



d



`








-



 



路



线



图



（



权



威



版



）



：



`



d



o



c



s



/



r



o



a



d



m



a



p



/



R



E



A



D



M



E



.



m



d



`








-



 



历



史



路



线



图



：



`



d



o



c



s



/



r



o



a



d



m



a



p



/



h



i



s



t



o



r



y



/



`








-



 



项



目



进



度



概



览



：



`



d



o



c



s



/



s



t



a



t



u



s



/



p



r



o



g



r



e



s



s



-



s



u



m



m



a



r



y



.



m



d



`








-



 



临



时



待



办



（



转



向



 



I



s



s



u



e



/



里



程



碑



前



的



过



渡



）



：



`



d



o



c



s



/



s



t



a



t



u



s



/



t



o



d



o



.



m



d



`













#



#



 



使



用



与



构



建








-



 



构



建



与



运



行



沙



盒



 



C



L



I



：



见



 



`



d



o



c



s



/



g



u



i



d



e



s



/



s



a



n



d



b



o



x



-



c



l



i



.



m



d



`



 



与



 



`



s



c



r



i



p



t



s



/



r



u



n



_



s



a



n



d



b



o



x



_



c



l



i



.



p



s



1



`



。








-



 



运



行



时



封



装



：



核



心



模



拟



逻



辑



以



 



`



g



e



n



e



s



i



s



_



r



u



n



t



i



m



e



`



 



动



态



库



形



式



对



外



提



供



 



A



P



I



，



前



端



二



进



制



（



如



 



C



L



I



/



G



U



I



/



G



a



m



e



）



复



用



同



一



接



口



。













#



#



 



文



档



约



定








-



 



路



线



图



以



 



`



d



o



c



s



/



r



o



a



d



m



a



p



/



R



E



A



D



M



E



.



m



d



`



 



为



权



威



来



源



。



历



史



分



篇



保



留



于



 



`



d



o



c



s



/



r



o



a



d



m



a



p



/



h



i



s



t



o



r



y



/



`



。








-



 



术



语



：



实



体



组



件



（



E



n



T



T



）



、



需



求



/



行



为



（



N



e



e



d



s



/



A



c



t



i



o



n



s



）



、



事



实



/



传



闻



（



F



a



c



t



/



R



u



m



o



r



）



、



快



照



（



S



i



m



u



l



a



t



i



o



n



S



n



a



p



s



h



o



t



）



。













#



#



 



下



一



步



建



议








-



 



将



 



`



d



o



c



s



/



s



t



a



t



u



s



/



t



o



d



o



.



m



d



`



 



中



仍



然



有



效



的



条



目



迁



移



到



 



I



s



s



u



e



 



与



 



M



i



l



e



s



t



o



n



e



，



按



 



`



d



o



c



s



/



r



o



a



d



m



a



p



/



R



E



A



D



M



E



.



m



d



`



 



的



阶



段



划



分



追



踪



。








-
 
世
界
模
型
概
览
：
`
d
o
c
s
/
a
r
c
h
i
t
e
c
t
u
r
e
/
W
O
R
L
D
_
M
O
D
E
L
.
m
d
`



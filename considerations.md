one thing thatll be good is if somewhere along the presentation, i was like well how do we do this right? we should use a!
but wait! google C++ style guide forbids this because xyz! instead we can do this and avoid consequences efg!

talk about a few things the attendees can actually remember when coding ie camelcase for types and name_spec for variables or whatever it was for google i dont remember

out of bounds buffer overflow
shunned by cybersecurity agencies
rewritten in C++



what is cmake ?
instead of 
g++ main.cpp foo.cpp bar.cpp -I... -L... -l... -std=c++20 ...

this is my executable
these are its source file
it needs C++20
it links to fmt
it includes this directory
it has these tests
So CMake gives you portability, structure, reproducibility, and scalability.

bjorne startstrup using profiles 
restricting unsafe memory management 
basically forcing c++ to convergently evolve to be closer to rust in terms of memory management safety
- ie enforcing smart pointers?
this is already part of the google c++ guide i think

the fact we are so deeply entrenched in c++
it might be more likely that the things be rewritten in a new modernized c++ than be completely rewritten in rust since at the end of the day, the ppl in charge of implementing that are lazy

so basically its not dying


tons of different ways to solve the same problem sojust cuz u know how to do it one way doesn't mean u are a cpp dev

for go there is only two ways to do it ask mehrdad about it 


leetcode traffic plateaud
- system design up 340%
- behavioural and mock system design is way up


alternatives to leetcode
live debugging session
- get buggy codebase id the cause and refactor

critique a pr
- id the mistkaks, solutuon and feedback

leetcode but layered
- leetcode but then they throw a curve ball
- something like what if u 

in that google mock interveiw the guy talked about 
how if u have a technical interview u give a olsution
verbalize the approach before the code
implement correctly and then 
discuss runtime 
they might also ask about what if this function gets called a million times and that onegets called like 5 times then 0(n^2) really becomes o(1) since its so rare, this is what we call amortized runtime
- adding constraints after u solve it like limited memory
- run it on a mobile device to try to expose those who just memorize the leetcode solution

in person super days for google and amazon
for final round (u cant use ai)


networking
they say soon enough 1 in 4 applicants will be fake, fake interview deepfake call alr been seen so a referral means more than ever, dennis take it away

behavioural is like 40% weight now,
15-20 STAR stories aligned to comapny values 
because their questions always are tied to those. for my final round behaioural i made a notion doc with collapsable possible q/a and practiced with mehrdad until i could answer the question no matter what angle it was asked from, ariel said try to find the qustion behind the question
parrot back how you demonstrate their company values through your lived experiences and do it eloquently

how does a url shortener work

the tradeoffs between
sql and nosql db

what is a load balancer and when to use one

and how caching improves performance



the tradeoffs between horizontal and vertical scaling where osmehting like creating replicas redundancy and it also allows for the possibility to scale infiintely as well as fault tolerance

if you had servers internationally then u could do something like useing hashing to bascially reroute the request to the nearest machien

cdn: for static content, copies to other cdn servers, 

cdns are one example of caching which is basically copying data so it can be refetched faster

an example of where this is useful is like a network request, theyre expensive so cacheing it to the disk is done to make it less expensive, but wait reading from the disk is also expensive so the computer copies it into memory, reading from the memory is expesivne so we copy it into ram, reading from ram is also expsieve so we go copy a subset into the cpu, which can be stored in the level 1 2 or 3 cache commonly referred to as l1 l2 l3 cache


what is grpc again, can it fit inot this project at all? 

google c++:
- tab at size two spaces (config tab key to be 2 spaces for workspace)
- no type deduction
- limit usage of dynamic memory to keep ownership with the code that allocated it, unique_ptr is preffered 


nasa space proof code:
- simple control flow ie no goto, setjmp, longjmp or recursion (are these assembly?)
- limit loops
- dont use the heap AT ALL
- datta integrity or data hiding 
- cehck all non void return values 
- limit the preprocessor
- conditional compilation ????
- no function pointer
- compile in pedantic mode -Wpedantic

maybe borrow the big list and big whiteboard from the library


we need to prioritize what to talk about or mention, some things are so unimportant or forgettable that we should not even bring it up in a 2hr long demo to uni level students. some things don't need to be brought up at all some things only need a sentence integrated into the rest of the presntation and some things are worth bothering to go into details. some of them are so obscure they are not relevant yk. 
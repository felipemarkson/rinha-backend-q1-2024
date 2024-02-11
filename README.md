# C - Rinha de Backend Q1 2024

Veja: [Por que C?](#id).

## Como rodar?
```
sudo docker compose up --build
```

## Como testar?
```
sudo docker compose up --build
./get_gatling.sh
./test_local.sh
```

## Dependencias externas

- [libpq](https://www.postgresql.org/docs/current/libpq.html)
- [liburing](https://github.com/axboe/liburing)
- [pgbouncer](https://www.pgbouncer.org/)

## Dependencias internas (compiladas junto com o binário)

- [picohhtpparser](https://github.com/h2o/picohttpparser)
- [cJSON](https://github.com/DaveGamble/cJSON)


## <a id="id"></a> Por que C?

```
From: Linus Torvalds <torvalds <at> linux-foundation.org>
Subject: Re: [RFC] Convert builin-mailinfo.c to use The Better String Library.
Newsgroups: gmane.comp.version-control.git
Date: 2007-09-06 17:50:28 GMT (2 years, 14 weeks, 16 hours and 36 minutes ago)

On Wed, 5 Sep 2007, Dmitry Kakurin wrote:
> 
> When I first looked at Git source code two things struck me as odd:
> 1. Pure C as opposed to C++. No idea why. Please don't talk about portability,
> it's BS.

*YOU* are full of bullshit.

C++ is a horrible language. It's made more horrible by the fact that a lot 
of substandard programmers use it, to the point where it's much much 
easier to generate total and utter crap with it. Quite frankly, even if 
the choice of C were to do *nothing* but keep the C++ programmers out, 
that in itself would be a huge reason to use C.

In other words: the choice of C is the only sane choice. I know Miles 
Bader jokingly said "to piss you off", but it's actually true. I've come 
to the conclusion that any programmer that would prefer the project to be 
in C++ over C is likely a programmer that I really *would* prefer to piss 
off, so that he doesn't come and screw up any project I'm involved with.

C++ leads to really really bad design choices. You invariably start using 
the "nice" library features of the language like STL and Boost and other 
total and utter crap, that may "help" you program, but causes:

 - infinite amounts of pain when they don't work (and anybody who tells me 
   that STL and especially Boost are stable and portable is just so full 
   of BS that it's not even funny)

 - inefficient abstracted programming models where two years down the road 
   you notice that some abstraction wasn't very efficient, but now all 
   your code depends on all the nice object models around it, and you 
   cannot fix it without rewriting your app.

In other words, the only way to do good, efficient, and system-level and 
portable C++ ends up to limit yourself to all the things that are 
basically available in C. And limiting your project to C means that people 
don't screw that up, and also means that you get a lot of programmers that 
do actually understand low-level issues and don't screw things up with any 
idiotic "object model" crap.

So I'm sorry, but for something like git, where efficiency was a primary 
objective, the "advantages" of C++ is just a huge mistake. The fact that 
we also piss off people who cannot see that is just a big additional 
advantage.

If you want a VCS that is written in C++, go play with Monotone. Really. 
They use a "real database". They use "nice object-oriented libraries". 
They use "nice C++ abstractions". And quite frankly, as a result of all 
these design decisions that sound so appealing to some CS people, the end 
result is a horrible and unmaintainable mess.

But I'm sure you'd like it more than git.

            Linus
```

## Copyrights

#### C - Rinha de Backend Q1 2024
Copyright 2024 Felipe Markson dos Santos Monteiro.

This software is licensed under the MIT License. See `LICENSE`.

#### picohttpparser

Copyright (c) 2009-2014 Kazuho Oku, Tokuhiro Matsuno, Daisuke Murase, Shigeo Mitsunari

The picohttpparser is licensed under either the MIT License or the Perl license.
See `picohttpparser/LICENSE`.

#### cJSON

Copyright (c) 2009-2017 Dave Gamble and cJSON contributors.

The cJSON is licensed under the MIT License
See `cJSON/LICENSE`
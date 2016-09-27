/* Copyright (c) 1998 Lucent Technologies - All rights reserved. */
#include "sam.h"

#define NSYSFILE    3
#define NOFILE      128

void
checkqid(File *f)
{
    int i, w;
    File *g;

    w = whichmenu(f);
    for(i=1; i<file.nused; i++){
        g = file.filepptr[i];
        if(w == i)
            continue;
        if(f->dev==g->dev && f->qid==g->qid)
            warn_SS(Wdupfile, &f->name, &g->name);
    }
}

void
writef(File *f)
{
    Rune c;
    Posn n;
    char *name;
    int i, samename, newfile;
    uint64_t dev, qid;
    int64_t mtime, appendonly, length;

    newfile = 0;
    samename = Strcmp(&genstr, &f->name) == 0;
    name = Strtoc(&f->name);
    i = statfile(name, &dev, &qid, &mtime, 0, 0);
    if(i == -1)
        newfile++;
    else if(samename &&
            (f->dev!=dev || f->qid!=qid || f->date<mtime)){
        f->dev = dev;
        f->qid = qid;
        f->date = mtime;
        warn_S(Wdate, &genstr);
        return;
    }
    if(genc)
        free(genc);
    genc = Strtoc(&genstr);
    if((io=create(genc, 1, 0666L)) < 0)
        error_s(Ecreate, genc);
    dprint("%s: ", genc);
    if(statfd(io, 0, 0, 0, &length, &appendonly) > 0 && appendonly && length>0)
        error(Eappend);
    n = writeio(f);
    if(f->name.s[0]==0 || samename)
        state(f, addr.r.p1==0 && addr.r.p2==f->nrunes? Clean : Dirty);
    if(newfile)
        dprint("(new file) ");
    if(addr.r.p2>0 && Fchars(f, &c, addr.r.p2-1, addr.r.p2) && c!='\n')
        warn(Wnotnewline);
    closeio(n);
    if(f->name.s[0]==0 || samename){
        if(statfile(name, &dev, &qid, &mtime, 0, 0) > 0){
            f->dev = dev;
            f->qid = qid;
            f->date = mtime;
            checkqid(f);
        }
    }
}

Posn
readio(File *f, int *nulls, int setdate)
{
    int n, b, w;
    Rune *r;
    Posn nt;
    Posn p = addr.r.p2;
    uint64_t dev, qid;
    int64_t mtime;
    char buf[BLOCKSIZE+1], *s;

    *nulls = FALSE;
    b = 0;
    for(nt = 0; (n = read(io, buf+b, BLOCKSIZE-b))>0; nt+=(r-genbuf)){
        n += b;
        b = 0;
        r = genbuf;
        s = buf;
        while(n > 0){
            if((*r = *(uchar*)s) < Runeself){
                if(*r)
                    r++;
                else
                    *nulls = TRUE;
                --n;
                s++;
                continue;
            }
            if(fullrune(s, n)){
                w = chartorune(r, s);
                if(*r)
                    r++;
                else
                    *nulls = TRUE;
                n -= w;
                s += w;
                continue;
            }
            b = n;
            memmove(buf, s, b);
            break;
        }
        Finsert(f, tmprstr(genbuf, r-genbuf), p);
    }
    if(b)
        *nulls = TRUE;
    if(*nulls)
        warn(Wnulls);
    if(setdate){
        if(statfd(io, &dev, &qid, &mtime, 0, 0) > 0){
            f->dev = dev;
            f->qid = qid;
            f->date = mtime;
            checkqid(f);
        }
    }
    return nt;
}

Posn
writeio(File *f)
{
    int m, n;
    Posn p = addr.r.p1;
    char *c;

    while(p < addr.r.p2){
        if(addr.r.p2-p>BLOCKSIZE)
            n = BLOCKSIZE;
        else
            n = addr.r.p2-p;
        if(Fchars(f, genbuf, p, p+n)!=n)
            panic("writef read");
        c = Strtoc(tmprstr(genbuf, n));
        m = strlen(c);
        if (m < n)
            panic("corrupted file");
        if(Write(io, c, m) != m){
            free(c);
            if(p > 0)
                p += n;
            break;
        }
        free(c);
        p += n;
    }
    return p-addr.r.p1;
}
void
closeio(Posn p)
{
    close(io);
    io = 0;
    if(p >= 0)
        dprint("#%lu\n", p);
}

int remotefd0 = 0;
int remotefd1 = 1;

void
bootterm(char *machine, char **argv, char **end)
{
    int ph2t[2], pt2h[2];

    if(machine){
        dup(remotefd0, 0);
        dup(remotefd1, 1);
        close(remotefd0);
        close(remotefd1);
        argv[0] = "samterm";
        *end = 0;
        execvp(samterm, argv);
        fprintf(stderr, "can't exec: ");
        perror(samterm);
        _exits("damn");
    }
    if(pipe(ph2t)==-1 || pipe(pt2h)==-1)
        panic("pipe");
    switch(fork()){
    case 0:
        dup(ph2t[0], 0);
        dup(pt2h[1], 1);
        close(ph2t[0]);
        close(ph2t[1]);
        close(pt2h[0]);
        close(pt2h[1]);
        argv[0] = "samterm";
        *end = 0;
        execvp(samterm, argv);
        fprintf(stderr, "can't exec: ");
        perror(samterm);
        _exits("damn");
    case -1:
        panic("can't fork samterm");
    }
    dup(pt2h[0], 0);
    dup(ph2t[1], 1);
    close(ph2t[0]);
    close(ph2t[1]);
    close(pt2h[0]);
    close(pt2h[1]);
}

void
connectto(char *machine)
{
    int p1[2], p2[2];

    if(pipe(p1)<0 || pipe(p2)<0){
        dprint("can't pipe\n");
        exits("pipe");
    }
    remotefd0 = p1[0];
    remotefd1 = p2[1];
    switch(fork()){
    case 0:
        dup(p2[0], 0);
        dup(p1[1], 1);
        close(p1[0]);
        close(p1[1]);
        close(p2[0]);
        close(p2[1]);
        execlp(getenv("RSH") ? getenv("RSH") : RXPATH, getenv("RSH") ? getenv("RSH") : RXPATH, machine, rsamname, "-R", (char*)0);
        dprint("can't exec %s\n", RXPATH);
        exits("exec");

    case -1:
        dprint("can't fork\n");
        exits("fork");
    }
    close(p1[1]);
    close(p2[0]);
}

void
startup(char *machine, int Rflag, char **arg, char **end)
{
    if(machine)
        connectto(machine);
    if(!Rflag)
        bootterm(machine, arg, end);
    downloaded = 1;
    outTs(Hversion, VERSION);
}

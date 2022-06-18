[system programming lecture]

-project 1 baseline

csapp.{c,h}
        CS:APP3e functions

shellex.c
        Simple shell example

1. 컴파일 및 실행 방법

$make
./shellex

2. 명령어

cd, cd..
ls
mkdir, rmdir
touch, cat, echo
exit
등과 같은 기본적인 쉘 명령어들을 실행한다.
cd 의 명령어는 직접 구현하였고, quotation이 있을때의 예외를 최대한 처리하려 하였다.

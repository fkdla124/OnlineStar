# 더미클라 테스트도중 서버 멈춤 문제발생
* 직접만든 링버퍼를 서버 메시지 송수신 과정에 사용하도록 코드수정 후 테스트 진행
* 로컬환경에서 서버, 더미클라 모두 실행
[1](./image/1.png)

* recvBuf.push()에서 문제 발생.
* 디버깅을 위해 문제 발생시 Sleep(INFINITE) 걸어놓음.

# VisualStudio 디버거로 프로세스 연결
[2](./image/2.png)

* push()가 0을 반환함. 직전 recv()의 반환이 -1(SOCKET_ERROR)인 것을 확인.

[3](./image/3.png)
* recv()쪽 errorCode = 10053 WSAECONNABORTED 확인.
* 더미클라에서 이미 closesocket()으로 종료한 세션에 recv() 하는 도중 발생한 것으로 추정.

# 해결
[4](./image/4.png)
* WSAECONNABORTED 발생시 세션 정보를 지우도록 지연삭제 마킹.
* 이후 테스트에서는 같은 상황이 발생하지 않음.

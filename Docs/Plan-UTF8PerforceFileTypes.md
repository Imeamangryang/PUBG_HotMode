# UTF-8 Perforce File Type Plan

## 결론

Perforce `unicode` 타입이 아니라 `utf8+D` 타입을 표준으로 사용한다.

- `utf8`: 워크스페이스 파일 자체를 항상 UTF-8로 유지
- `+D`: 소스코드에 적합한 RCS delta 저장 강제
- `unicode`: `P4CHARSET`에 따라 워크스페이스 파일이 CP949 등으로 변환될 수 있으므로 사용하지 않음

## 목표

- `Config`, `Content`, `Docs`, `Source` 안의 모든 텍스트 파일을 UTF-8 파일 내용으로 정규화
- 루트의 `.gitignore`, `.p4ignore`, `.vsconfig`, `.uproject`, `.md`, `.txt`, `.json` 등 텍스트 파일을 UTF-8로 정규화
- 기존 depot 텍스트 파일의 Perforce filetype을 `utf8+D`로 변경
- 앞으로 추가되는 텍스트 파일이 자동으로 `utf8+D`로 add되도록 typemap 구성
- `Content`의 `.uasset`, `.umap` 등 바이너리 에셋은 변경하지 않음

## 전제

- Perforce 서버는 Unicode mode로 동작 중이어야 함
- 팀 클라이언트는 `P4CHARSET=utf8` 권장
- `p4 typemap -i` 권한이 필요함. 권한이 없으면 Perforce admin이 typemap 적용
- 현재 opened 파일은 별도 확인 후 충돌 없이 처리

## Typemap 초안

기존 typemap이 비어 있으므로 아래 항목을 프로젝트 depot 경로에 추가한다.

```text
TypeMap:
    binary+l //depot/PUBG_HotMode/....uasset
    binary+l //depot/PUBG_HotMode/....umap

    utf8+D //depot/PUBG_HotMode/....h
    utf8+D //depot/PUBG_HotMode/....hpp
    utf8+D //depot/PUBG_HotMode/....c
    utf8+D //depot/PUBG_HotMode/....cpp
    utf8+D //depot/PUBG_HotMode/....cs
    utf8+D //depot/PUBG_HotMode/....ini
    utf8+D //depot/PUBG_HotMode/....uproject
    utf8+D //depot/PUBG_HotMode/....uplugin
    utf8+D //depot/PUBG_HotMode/....json
    utf8+D //depot/PUBG_HotMode/....md
    utf8+D //depot/PUBG_HotMode/....txt
    utf8+D //depot/PUBG_HotMode/....csv
    utf8+D //depot/PUBG_HotMode/....tsv
    utf8+D //depot/PUBG_HotMode/....xml
    utf8+D //depot/PUBG_HotMode/....yml
    utf8+D //depot/PUBG_HotMode/....yaml
    utf8+D //depot/PUBG_HotMode/....usf
    utf8+D //depot/PUBG_HotMode/....ush
    utf8+D //depot/PUBG_HotMode/....gitignore
    utf8+D //depot/PUBG_HotMode/....p4ignore
    utf8+D //depot/PUBG_HotMode/....p4config
    utf8+D //depot/PUBG_HotMode/....vsconfig
```

`utf8`만 쓰면 기존 파일의 `+C` 저장 modifier가 보존될 수 있으므로, 소스코드 표준화 작업에서는 `utf8+D`를 명시한다.

## 작업 순서

### Task 1: 대상 파일 목록 생성

Description: `Config`, `Content`, `Docs`, `Source`, 루트 텍스트 파일을 대상으로 Perforce tracked 파일 목록을 만든다. `Content`는 확장자와 실제 바이트 검사를 함께 사용해 텍스트만 선별한다.

Acceptance:
- `.uasset`, `.umap`, 이미지, 사운드, 모델 등 바이너리 파일 제외
- 삭제된 depot revision 제외
- 현재 opened 파일과 다른 사용자가 opened한 파일 목록 별도 출력

Verification:
```powershell
p4 opened -a //depot/PUBG_HotMode/...
p4 -ztag fstat -T depotFile,headType //depot/PUBG_HotMode/Config/... //depot/PUBG_HotMode/Content/... //depot/PUBG_HotMode/Docs/... //depot/PUBG_HotMode/Source/...
```

Dependencies: none

### Task 2: UTF-8 변환 스크립트로 내용 정규화

Description: 후보 텍스트 파일을 strict UTF-8로 읽고, 실패하는 파일만 CP949 등 기존 인코딩으로 디코드한 뒤 UTF-8 without BOM으로 저장한다. 이미 UTF-8인 파일은 내용 변경하지 않는다.

Acceptance:
- strict UTF-8 검증 성공
- `\uFFFD` 대체 문자 없음
- 줄바꿈 정책 유지
- 바이너리 파일 미수정

Verification:
```powershell
# strict UTF-8 scan
# replacement char scan
rg -n "\\uFFFD|占|怨|蹂|濡|洹|罹" Config Content Docs Source .gitignore .p4ignore .vsconfig PUBG_HotMode.uproject
```

Dependencies: Task 1

### Task 3: 기존 파일 Perforce 타입 변경

Description: 정규화된 텍스트 파일을 dedicated changelist에 열고 filetype을 `utf8+D`로 변경한다. 이미 열린 파일은 사용자 작업과 섞이지 않도록 처리 방침을 먼저 정한다.

Acceptance:
- 대상 텍스트 파일의 pending type이 `utf8+D` 또는 표시상 `utf8` 계열
- 바이너리 파일 타입 변경 없음
- 다른 사용자 opened 파일은 강제 처리하지 않음

Verification:
```powershell
p4 change
p4 -x utf8-file-list.txt edit -c <CL> -t utf8+D
p4 opened -c <CL>
p4 -ztag fstat -T depotFile,type,headType -Ro @=<CL>
```

Dependencies: Task 2

### Task 4: Typemap 적용

Description: typemap에 UTF-8 텍스트 규칙과 Unreal 바이너리 에셋 규칙을 추가한다. 기존 typemap이 생긴 경우 병합해서 적용한다.

Acceptance:
- 새 `.h`, `.cpp`, `.ini`, `.md`, `.uproject`, ignore 계열 파일 add 시 `utf8+D` 적용
- 새 `.uasset`, `.umap` add 시 `binary+l` 적용

Verification:
```powershell
p4 typemap -o
# 임시 파일 p4 add -n 또는 테스트 workspace에서 add preview
```

Dependencies: Task 1

### Task 5: 빌드 및 P4V 표시 검증

Description: 타입 변경 후 UE 빌드와 P4V 표시를 확인한다.

Acceptance:
- `PUBG_HotModeEditor Win64 Development` 빌드 성공
- P4V에서 한글 주석이 깨지지 않음
- `p4 diff`에서 한글 diff 정상 표시

Verification:
```powershell
& "C:\Program Files\Epic Games\UE_5.7_Source\Engine\Build\BatchFiles\Build.bat" PUBG_HotModeEditor Win64 Development -Project="C:\Users\me\source\UE5\PUBG_HotMode\PUBG_HotMode.uproject" -WaitMutex
p4 diff -du
```

Dependencies: Task 3, Task 4

## 리스크

- `p4 typemap -i` 권한 부족 가능성
- 이미 열린 사용자 변경과 filetype 변경이 섞일 가능성
- CP949로 저장된 파일 중 CP949에 없는 문자가 이미 손실된 경우 원문 복구 불가
- `Content` 폴더에는 바이너리가 많아 확장자 기반 일괄 변경 금지
- `unicode` 타입으로 바꾸면 depot은 UTF-8이어도 워크스페이스 파일이 `P4CHARSET`에 따라 CP949 등으로 바뀔 수 있음

## 권장 실행 방침

1. typemap은 `utf8+D` 기준으로 먼저 적용
2. 기존 파일은 generated candidate list 기반으로만 변경
3. 현재 opened 파일은 별도 changelist로 정리하거나 사용자가 먼저 submit/revert한 뒤 처리
4. 빌드와 P4V 표시 확인 후 submit

# Vdjm Record Event Flow DataAsset Editor Guide

## 목적
- `UVdjmRecordEventFlowDataAsset`을 기본 Details 패널보다 빠르게 검토하기 위한 editor 전용 창이다.
- Runtime 구조를 바꾸지 않고, DataAsset을 source of truth로 유지한다.
- JSON을 생성/검증/복사하거나, 필요 시 JSON을 다시 DataAsset에 import하는 보조 도구다.

## 여는 방법
- Content Browser에서 `Vdjm Record Event Flow DataAsset`을 더블클릭한다.
- `Vdjm Record Event Flow` 전용 에디터가 열리면 `Details`, `JSON`, `Summary`, `Guide` 탭을 사용한다.

## 탭 설명
- `Details`: 기존 DataAsset 편집 화면이다. `Root Events`, `Subgraphs`, 각 event node의 값을 여기서 수정한다.
- `JSON`: 현재 DataAsset을 flow JSON으로 출력한다.
- `Summary`: root/subgraph의 이벤트 목록과 주요 `Tag`, `RuntimeSlotKey`, `ContextKey`, `SignalTag`, `SubgraphTag`를 한눈에 본다.
- `Guide`: DataAsset 안의 `FlowAuthoringGuide`와 `FlowAuthorNotes`를 읽기 좋게 보여준다.

## JSON 탭 버튼
- `Refresh JSON`: 현재 DataAsset 상태를 다시 JSON으로 출력한다.
- `Copy JSON`: JSON 텍스트를 클립보드에 복사한다. Codex에게 보여주거나 외부에서 편집할 때 쓴다.
- `Validate JSON`: 현재 JSON이 import 가능한지 검사한다.
- `Import JSON To Asset`: 현재 JSON으로 DataAsset의 `Root Events`와 `Subgraphs`를 덮어쓴다. 확인창을 거친다.

## 추천 사용 흐름
1. `Details` 탭에서 Root Events와 Subgraphs를 수정한다.
2. `Summary` 탭에서 `Refresh Summary`로 흐름을 빠르게 점검한다.
3. `JSON` 탭에서 `Refresh JSON` 후 `Copy JSON`을 누른다.
4. JSON을 채팅이나 외부 도구에 붙여넣어 검토한다.
5. 외부에서 수정한 JSON을 되돌릴 때만 `Import JSON To Asset`을 사용한다.

## 주의점
- `Import JSON To Asset`은 DataAsset 내용을 덮어쓴다. 사용 전 asset 저장 상태를 확인한다.
- 전용 에디터는 editor 모듈에만 존재하므로 Android/패키징 런타임에는 포함되지 않아야 한다.
- 진짜 노드 그래프 편집기는 아직 아니다. 현재 1차 목표는 DataAsset 작성/검토/복붙 UX 개선이다.

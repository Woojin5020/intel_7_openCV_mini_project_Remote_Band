import cv2
import numpy as np

# ---- 설정: ROI 사각형 (x, y, w, h) ----
ROI = (200, 120, 300, 220)   # 필요에 맞게 바꾸세요
MOTION_PIXELS_TH = 800       # 움직임 픽셀 개수 임계값(민감도)

cap = cv2.VideoCapture(0)    # 웹캠: /dev/video*이면 번호 바꾸세요
if not cap.isOpened():
    raise RuntimeError("Camera open failed")

prev_gray = None

while True:
    ret, frame = cap.read()
    if not ret:
        break

    # 프레임 전처리(그레이 + 블러)
    gray = cv2.cvtColor(frame, cv2.COLOR_BGR2GRAY)
    gray = cv2.GaussianBlur(gray, (5, 5), 0)

    # 첫 프레임이면 기준 저장만
    if prev_gray is None:
        prev_gray = gray.copy()
        # 사각형을 그려 보여주기만
        x, y, w, h = ROI
        cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)
        cv2.imshow("Motion ROI", frame)
        if cv2.waitKey(1) & 0xFF == 27:  # ESC 종료
            break
        continue

    # 프레임 차이로 움직임 추정
    diff = cv2.absdiff(gray, prev_gray)
    _, thresh = cv2.threshold(diff, 25, 255, cv2.THRESH_BINARY)
    thresh = cv2.medianBlur(thresh, 5)

    # ROI 잘라서 움직임 픽셀 수 집계
    x, y, w, h = ROI
    roi_mask = thresh[y:y+h, x:x+w]
    motion_pixels = int(np.count_nonzero(roi_mask))

    # 시각화: 움직임이면 빨간색으로 채우고, 아니면 녹색 테두리
    overlay = frame.copy()
    if motion_pixels > MOTION_PIXELS_TH:
        cv2.rectangle(overlay, (x, y), (x+w, y+h), (0, 0, 255), -1)  # 빨간색 채우기
        # 원본과 합성(투명도 0.35)
        frame = cv2.addWeighted(overlay, 0.35, frame, 0.65, 0)
    else:
        cv2.rectangle(frame, (x, y), (x+w, y+h), (0, 255, 0), 2)

    # 디버그 정보(원하면 주석 처리)
    cv2.putText(frame, f"motion_pixels={motion_pixels}", (10, 30),
                cv2.FONT_HERSHEY_SIMPLEX, 0.8, (255, 255, 255), 2, cv2.LINE_AA)

    cv2.imshow("Motion ROI", frame)

    # 다음 비교를 위해 현재 프레임 저장(느린 변화에 덜 민감하게 약간 가중 평균도 가능)
    prev_gray = gray

    key = cv2.waitKey(1) & 0xFF
    if key == 27:  # ESC
        break

cap.release()
cv2.destroyAllWindows()


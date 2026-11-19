import cv2

# Целевая высота processed изображения
TARGET_HEIGHT = 240

# ВАЖНО: Укажите здесь ТОЛЬКО те символы, которые реально могут быть на номере.
ALLOWED_LETTERS = 'ABCEHKMOPTXY'
ALLOWED_NUMBERS = '0123456789'
ALLOWED_CHARS = ALLOWED_NUMBERS + ALLOWED_LETTERS

def preprocess_image(img):
    """Увеличивает, переводит в Ч/Б и повышает контраст"""
    # Увеличение
    h, w = img.shape[:2]    
    target_width = int(round((TARGET_HEIGHT / h) * w))
    if h > TARGET_HEIGHT:
        img = cv2.resize(img, dsize = (target_width, TARGET_HEIGHT), interpolation=cv2.INTER_AREA)
    else:
        img = cv2.resize(img, dsize = (target_width, TARGET_HEIGHT), interpolation=cv2.INTER_CUBIC)

    # Перевод в оттенки серого
    gray = cv2.cvtColor(img, cv2.COLOR_BGR2GRAY)

    # Бинаризация Оцу
    _, thresh = cv2.threshold(gray, 0, 255, cv2.THRESH_BINARY + cv2.THRESH_OTSU)

    return thresh

def replace_symbol(text, index, symbol):
    temp = list(text)
    temp[index] = symbol
    return ''.join(temp)

def fix_common_mistakes(text):
    """Постобработка: исправляет частые ошибки OCR для номеров"""
    text = text.upper().replace(' ', '').replace('-', '')

    for i in range(len(text)):
        symbol = text[i]
        if i >= 1 and i <= 3 and symbol in ALLOWED_LETTERS:
            match symbol:
                case 'B':
                    text = replace_symbol(text, i, '8')
                case 'S':
                    text = replace_symbol(text, i, '5')
                case 'O':
                    text = replace_symbol(text, i, '0')
                case 'T':
                    text = replace_symbol(text, i, '1')
        elif (i < 1 or i > 3) and symbol in ALLOWED_NUMBERS:
            match symbol:
                case '8':
                    text = replace_symbol(text, i, 'B')
                case '5':
                    text = replace_symbol(text, i, 'S')
                case '0':
                    text = replace_symbol(text, i, 'O')
                case '1':
                    text = replace_symbol(text, i, 'T')
    return text

def recognition(image, yolo, ocr, conf=0.5):
    """image - изображение на распознавание номера \n
    model - файл с весами YOLO \n
    gpu - True/False имеется ли видеокарта NVIDIA"""

    results = yolo(image, conf=conf) #, verbose=False)

    boxes = results[0].boxes 
    if len(boxes) == 0:
        return "" 
    
    coords = boxes.xyxy[0].tolist()

    cropped_image = image[round(coords[1]):round(coords[3]), round(coords[0]):round(coords[2])]

    processed_img = preprocess_image(cropped_image)

    results = ocr.readtext(
        processed_img, 
        allowlist=ALLOWED_CHARS,
        detail=0, 
        paragraph=True        
    )

    if not results:
        return ""

    raw_text = ''.join(results)
    final_text = fix_common_mistakes(raw_text)

    return final_text
    
if __name__ == '__main__':
    recognition()
import serial
import sys
import threading

# Dicionário Internacional de Código Morse
MORSE_DICT = {
    'A': '.-', 'B': '-...', 'C': '-.-.', 'D': '-..', 'E': '.', 
    'F': '..-.', 'G': '--.', 'H': '....', 'I': '..', 'J': '.---', 
    'K': '-.-', 'L': '.-..', 'M': '--', 'N': '-.', 'O': '---', 
    'P': '.--.', 'Q': '--.-', 'R': '.-.', 'S': '...', 'T': '-', 
    'U': '..-', 'V': '...-', 'W': '.--', 'X': '-..-', 'Y': '-.--', 
    'Z': '--..', '0': '-----', '1': '.----', '2': '..---', 
    '3': '...--', '4': '....-', '5': '.....', '6': '-....', 
    '7': '--...', '8': '---..', '9': '----.'
}
# Dicionário Reverso (Morse -> Texto)
REVERSE_DICT = {value: key for key, value in MORSE_DICT.items()}

def texto_para_morse(texto):
    palavras = texto.upper().split()
    morse_palavras = []
    for palavra in palavras:
        letras_morse = [MORSE_DICT.get(letra, '') for letra in palavra]
        morse_palavras.append(' '.join(letras_morse))
    return ' / '.join(morse_palavras) + '\n'

def decodifica_linha_morse(linha_morse):
    # O STM32 manda palavras separadas por '/' e letras por ' '
    texto_claro = ""
    palavras = linha_morse.strip().split('/')
    for palavra in palavras:
        letras = palavra.strip().split(' ')
        for letra in letras:
            texto_claro += REVERSE_DICT.get(letra, '?')
        texto_claro += " "
    return texto_claro.strip()

def escuta_serial(porta_serial):
    print(f"\n[+] Escutando a porta {porta_serial.name}...")
    while True:
        try:
            if porta_serial.in_waiting > 0:
                # O STM32 termina a transmissão com \r\n
                linha = porta_serial.readline().decode('utf-8').strip()
                if linha:
                    texto = decodifica_linha_morse(linha)
                    print(f"\n[RECEBIDO MORSE]: {linha}")
                    print(f"[TRADUZIDO]: {texto}\n> ", end="")
        except Exception as e:
            print(f"\nErro de leitura: {e}")
            break

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Uso: python chat_morse.py <PORTA_SERIAL>")
        sys.exit(1)

    porta = sys.argv[1]
    
    try:
        # Abre a porta virtual gerada pelo USB CDC
        ser = serial.Serial(porta, 115200, timeout=1)
    except Exception as e:
        print(f"Erro ao abrir a porta {porta}.")
        sys.exit(1)

    # Inicia a Thread que fica lendo o STM32 em background
    thread_leitura = threading.Thread(target=escuta_serial, args=(ser,), daemon=True)
    thread_leitura.start()

    print("=== CHAT ÓPTICO STM32 INICIADO ===")
    print("Digite sua mensagem e pressione ENTER (ou 'sair' para encerrar).")
    
    while True:
        try:
            msg = input("> ")
            if msg.lower() == 'sair':
                break
            if msg:
                morse_string = texto_para_morse(msg)
                # Envia os pontos e traços formatados pro STM32 piscar
                ser.write(morse_string.encode('utf-8'))
        except KeyboardInterrupt:
            break
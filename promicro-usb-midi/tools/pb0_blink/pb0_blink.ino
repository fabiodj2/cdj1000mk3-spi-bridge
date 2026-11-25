// Teste do PB0 (pad do LED RX) na Pro Micro 5V.
// Alterna o PB0 entre 0 V e 5 V a cada 3 s. Meça os pads perto do LED RX com o multimetro em DC:
// o pad ligado ao PB0 (lado do resistor "331" voltado ao chip) alterna 0 V <-> 5 V.
// Depois do teste, REGRAVE o firmware do deck (deck1/cdj_deck1 ou deck2/cdj_deck2).
void setup() {
  DDRB |= _BV(0);          // PB0 como saida
}
void loop() {
  PORTB ^= _BV(0);         // inverte PB0
  delay(3000);
}

#include <HCSR04.h>
#include <LCD_I2C.h>
#include <AccelStepper.h>

// --- Constantes matérielles ---
const int BROCHE_DECLENCHEUR = 11;
const int BROCHE_ECHO = 12;
const int BROCHE_BUZZER = 22;
const int BROCHE_ROUGE = 10;
const int BROCHE_VERT = 9;
const int BROCHE_BLEU = 8;
const int TYPE_INTERFACE_MOTEUR = 4;
const int BROCHE_IN1 = 39;
const int BROCHE_IN2 = 41;
const int BROCHE_IN3 = 43;
const int BROCHE_IN4 = 45;

// --- Constantes d’angles ---
const int ANGLE_MINIMUM = 10;
const int ANGLE_MAXIMUM = 170;
const float PAS_PAR_TOUR = 2038.0;
const long POSITION_MIN = (ANGLE_MINIMUM * PAS_PAR_TOUR) / 360;
const long POSITION_MAX = (ANGLE_MAXIMUM * PAS_PAR_TOUR) / 360;

// --- Constantes seuils et délais ---
const float DISTANCE_SEUIL_OUVERTURE = 30.0;
const float DISTANCE_SEUIL_FERMETURE = 60.0;
const float DISTANCE_SEUIL_ALARME = 15.0;
const int INTERVALLE_MESURE_DISTANCE = 50;
const int INTERVALLE_MISE_A_JOUR_LCD = 100;
const int INTERVALLE_MISE_A_JOUR_SERIE = 100;
const int INTERVALLE_CLIGNOTEMENT_GYROPHARE = 150;
const int DELAI_SONNERIE_ALARME = 3000;
const int DELAI_EXTINCTION_GYROPHARE = 3000;

HCSR04 capteur(BROCHE_DECLENCHEUR, BROCHE_ECHO);
LCD_I2C lcd(0x27, 16, 2);
AccelStepper moteur(TYPE_INTERFACE_MOTEUR, BROCHE_IN1, BROCHE_IN3, BROCHE_IN2, BROCHE_IN4);

const String numeroEtudiant = "2409626";

int angleActuel = 0;

enum EtatPorte { FERMEE, OUVERTURE, OUVERTE, FERMETURE } etatPorte = FERMEE;
enum EtatAlerte { ALARME_DESACTIVEE, ALARME_ACTIVEE } etatAlerte = ALARME_DESACTIVEE;

// --- Variables de gestion ---
float distance = 0.0;
unsigned long dernierTempsDistance = 0;
unsigned long dernierTempsAffichage = 0;
unsigned long dernierTempsGyrophare = 0;
unsigned long dernierTempsAlerte = 0;
unsigned long tempsChangementEtat = 0;
bool gyrophareRouge = true;
bool porteEnMouvement = false;
bool extinctionDifferee = false;
unsigned long tempsExtinction = 0;

void setup() {
  Serial.begin(115200);
  lcd.begin();
  lcd.backlight();
  moteur.setMaxSpeed(500);
  moteur.setAcceleration(200);
  moteur.setSpeed(500);
  moteur.moveTo(POSITION_MIN);
  moteur.enableOutputs();

  pinMode(BROCHE_BUZZER, OUTPUT);
  pinMode(BROCHE_ROUGE, OUTPUT);
  pinMode(BROCHE_VERT, OUTPUT);
  pinMode(BROCHE_BLEU, OUTPUT);

  lcd.setCursor(0, 0);
  lcd.print(numeroEtudiant);
  lcd.setCursor(0, 1);
  lcd.print("    LAB 4A    ");
  delay(2000);
  lcd.clear();
}

void loop() {
  unsigned long maintenant = millis();

  gererDistance(maintenant);
  gererEtatPorte(maintenant);
  gererAlerte(maintenant);
  gererAffichage(maintenant);
  gererSerie(maintenant);

  if (porteEnMouvement) {
    moteur.enableOutputs();
    moteur.run();
    if (!moteur.isRunning()) {
      porteEnMouvement = false;
      moteur.disableOutputs();
    }
  }
}

void gererDistance(unsigned long temps) {
  if (temps - dernierTempsDistance >= INTERVALLE_MESURE_DISTANCE) {
    distance = capteur.dist();
    dernierTempsDistance = temps;
  }
}

void gererEtatPorte(unsigned long temps) {
  if (etatAlerte == ALARME_ACTIVEE) return;

  if (etatPorte == FERMEE) {
    mettreCouleurLEDs(HIGH, LOW, LOW);

    if (distance < DISTANCE_SEUIL_OUVERTURE && distance > 0) {
      etatPorte = OUVERTURE;
      moteur.moveTo(POSITION_MAX);
      porteEnMouvement = true;
      tempsChangementEtat = temps;
    }
  }

  if (etatPorte == OUVERTURE) {
    mettreCouleurLEDs(LOW, LOW, HIGH);

    if (!porteEnMouvement && moteur.distanceToGo() == 0) {
      etatPorte = OUVERTE;
      tempsChangementEtat = temps;
    }
  }

  if (etatPorte == OUVERTE) {
    mettreCouleurLEDs(LOW, HIGH, LOW);

    if (distance > DISTANCE_SEUIL_FERMETURE) {
      etatPorte = FERMETURE;
      moteur.moveTo(POSITION_MIN);
      porteEnMouvement = true;
      tempsChangementEtat = temps;
    }
  }

  if (etatPorte == FERMETURE) {
    mettreCouleurLEDs(LOW, LOW, HIGH);

    if (!porteEnMouvement && moteur.distanceToGo() == 0) {
      etatPorte = FERMEE;
      tempsChangementEtat = temps;
    }
  }
}

void gererAlerte(unsigned long temps) {
  if (etatAlerte == ALARME_DESACTIVEE && distance <= DISTANCE_SEUIL_ALARME && distance > 0) {
    digitalWrite(BROCHE_BUZZER, HIGH);
    etatAlerte = ALARME_ACTIVEE;
    dernierTempsAlerte = temps;
    extinctionDifferee = false;
  }

  if (etatAlerte == ALARME_ACTIVEE) {
    if (temps - dernierTempsGyrophare >= INTERVALLE_CLIGNOTEMENT_GYROPHARE) {
      clignoterGyrophare();
      dernierTempsGyrophare = temps;
    }

    if (temps - dernierTempsAlerte >= DELAI_SONNERIE_ALARME) {
      digitalWrite(BROCHE_BUZZER, LOW);
      etatAlerte = ALARME_DESACTIVEE;
      extinctionDifferee = true;
      tempsExtinction = temps;
    }
  }

  if (extinctionDifferee && (temps - tempsExtinction >= DELAI_EXTINCTION_GYROPHARE)) {
    eteindreGyrophare();
    mettreCouleurLEDs(LOW, LOW, LOW);
    extinctionDifferee = false;
  }
}

void clignoterGyrophare() {
  if (gyrophareRouge) {
    digitalWrite(BROCHE_ROUGE, HIGH);
    digitalWrite(BROCHE_BLEU, LOW);
  } else {
    digitalWrite(BROCHE_ROUGE, LOW);
    digitalWrite(BROCHE_BLEU, HIGH);
  }
  digitalWrite(BROCHE_VERT, LOW);
  gyrophareRouge = !gyrophareRouge;
}

void eteindreGyrophare() {
  digitalWrite(BROCHE_ROUGE, LOW);
  digitalWrite(BROCHE_VERT, LOW);
  digitalWrite(BROCHE_BLEU, LOW);
}

void mettreCouleurLEDs(bool r, bool v, bool b) {
  if (etatAlerte == ALARME_DESACTIVEE && !extinctionDifferee) {
    digitalWrite(BROCHE_ROUGE, r);
    digitalWrite(BROCHE_VERT, v);
    digitalWrite(BROCHE_BLEU, b);
  }
}

void gererAffichage(unsigned long temps) {
  if (temps - dernierTempsAffichage >= INTERVALLE_MISE_A_JOUR_LCD) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Dist : ");
    lcd.print(distance);
    lcd.print(" cm");

    lcd.setCursor(0, 1);
    if (etatPorte == FERMEE) lcd.print("Porte : Fermee");
    if (etatPorte == OUVERTE) lcd.print("Porte : Ouverte");
    if (etatPorte == OUVERTURE || etatPorte == FERMETURE) {
      angleActuel = map(moteur.currentPosition(), POSITION_MIN, POSITION_MAX, ANGLE_MINIMUM, ANGLE_MAXIMUM);
      lcd.print("Angle : ");
      lcd.print(angleActuel);
      lcd.print((char)223);
    }
    dernierTempsAffichage = temps;
  }
}

void gererSerie(unsigned long temps) {
  static unsigned long dernierTempsSerie = 0;
  if (temps - dernierTempsSerie >= INTERVALLE_MISE_A_JOUR_SERIE) {
    Serial.print("etd:");
    Serial.print(numeroEtudiant);
    Serial.print(", dist:");
    Serial.print(distance);
    Serial.print(", angle:");
    Serial.println(angleActuel);
    dernierTempsSerie = temps;
  }
}

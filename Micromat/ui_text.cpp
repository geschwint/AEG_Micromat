#include "ui_text.h"
#include "settings.h"

static const char *const ENGLISH[] = {
  "OPEN",                                       // Open
  "End",                                        // End
  "Paused",                                     // Paused
  "Timer",                                      // Timer
  "Menu",                                       // Menu
  "Settings",                                   // Settings
  "Defrost",                                    // Defrost
  "Programs",                                   // Programs
  "Clock",                                      // Clock
  "Set time",                                   // SetTime
  "Auto time (Wi-Fi)",                          // AutoTime
  "Brightness",                                 // Brightness
  "Dimmed level",                               // DimLevel
  "Dim after",                                  // DimDelay
  "Never",                                      // Never
  "Sound",                                      // Sound
  "Key clicks",                                 // KeyClicks
  "Volume",                                     // Volume
  "Low",                                        // Low
  "Medium",                                     // Medium
  "High",                                       // High
  "On",                                         // On
  "Off",                                        // Off
  "Language",                                   // Language
  "Meat",                                       // Meat
  "Poultry",                                    // Poultry
  "Fish",                                       // Fish
  "Bread",                                      // Bread
  "Reheat plate",                               // ReheatPlate
  "Beverage",                                   // Beverage
  "Popcorn",                                    // Popcorn
  "Soften butter",                              // SoftenButter
  "Melt chocolate",                             // MeltChocolate
  "Favourite",                                  // Favourite
  "Save as favourite",                          // SaveFavourite
  "Empty",                                      // Empty
  "Set a time first",                           // SetTimeFirst
  "Saved",                                      // Saved
  "Turn the food",                              // TurnFood
  "Wi-Fi is off or has no network",             // NoWifi
  "Turn + push, or tap   Stop: back",           // MenuHint
  "Turn: set   Push: OK   Stop: back",          // EditHint
  "Turn: set   Push: next   Tap: field",        // ClockHint
  "Service",                                    // Service
  "Stop/Program/push: exit",                    // ServiceHint
  "Wi-Fi",                                      // Wifi
  "Network",                                    // Network
  "Not set",                                    // NotSet
  "Forget network",                             // ForgetNetwork
  "Network forgotten",                          // Forgotten
  "Security key could not be created",          // NoKey
  "Wi-Fi setup",                                // WifiSetup
  "Password",                                   // Password
  "Address",                                    // Address
  "Scan the code with your phone",              // SetupScan
  "Phone connected: pick your network",         // SetupPhone
  "Testing %s...",                              // SetupTesting
  "Connected and saved",                        // SetupOk
  "Network not found",                          // SetupNotFound
  "Could not connect: wrong password?",         // SetupNoConnect
  "Could not save the password",                // SetupSaveFailed
  "Setup timed out",                            // SetupTimeout
  "Stop: cancel",                               // SetupHint
  "Stop/push: back",                            // SetupDoneHint
  "Choose your network",                        // PageChoose
  "Other network (name)",                       // PageOther
  "Connect",                                    // PageConnect
  "Search again",                               // PageRescan
  "Searching for networks...",                  // PageSearching
  "Testing the connection...",                  // PageTesting
  "Connected! The network is saved. You can close this page.",  // PageOk
  "Try again",                                  // PageTryAgain
  "Firmware update",                            // FirmwareUpdate
  "Not while cooking",                          // NotWhileCooking
  "Open the address and enter the password",    // UpdateSignIn
  "On your network: %s",                        // UpdateHome
  "Receiving... %u %%",                         // UpdateReceiving
  "Installed, restarting...",                   // UpdateDone
  "Update failed: %s",                          // UpdateFailed
  "Closed (no activity)",                       // UpdateClosed
  "Running version",                            // PageVersion
  "Firmware file (.bin)",                       // PageFile
  "Install",                                    // PageInstall
  "Takes about 30 s. Keep this page open; the oven restarts when done.",  // PageUpdateNote
  "Installed. The oven restarts now.",          // PageUpdateOk
  "Update failed:",                             // PageUpdateFailed
  "Sign in",                                    // PageSignIn
  "Wrong password",                             // PageWrongPassword
  "If no file picker opens here, open http://192.168.4.1 in your normal browser (Chrome, Safari).",  // PageBrowserHint
  "Debug log",                                  // DebugLog
  "Logging",                                    // Logging
  "Download",                                   // DownloadLogs
  "No SD card",                                 // NoSdCard
  "No log files yet.",                          // PageNoLogs
  "Delete logs",                                // PageDeleteLogs
  "Logs deleted.",                              // PageLogsDeleted
  "About",                                      // About
  "Firmware",                                   // AboutFirmware
  "Date",                                       // AboutDate
  "Build",                                      // AboutBuild
  "Oven",                                       // AboutOven
  "Model",                                      // AboutOvenModel
  "Type",                                       // AboutType
  "E-Nr.",                                      // AboutENr
  "Serial no.",                                 // AboutSerial
  "Board",                                      // AboutBoard
  "Model",                                      // AboutModel
  "Chip",                                       // AboutChip
  "CPU",                                        // AboutCpu
  "Flash",                                      // AboutFlash
  "PSRAM",                                      // AboutPsram
  "Display",                                    // AboutDisplay
  "Touch",                                      // AboutTouch
  "Clock chip",                                 // AboutClockChip
  "Relays",                                     // AboutRelays
  "Magnetron",                                  // AboutMagnetron
  "LVGL",                                       // AboutLvgl
  "Arduino core",                               // AboutCore
  "orig. relay",                                // OriginalRelay
  "IP address",                                 // AboutIp
  "Last IP",                                    // AboutLastIp
  "MAC",                                        // AboutMac
  "Connected",                                  // Connected
  "Easter eggs",                                // EasterEggs
  "End melody",                                 // EggMelody
  "Authors",                                    // EggAuthors
  "Stop: back   Close the door: restart",       // EggHint
};

static const char *const SWEDISH[] = {
  "ÖPPEN",                                      // Open
  "Klar",                                       // End
  "Paus",                                       // Paused
  "Timer",                                      // Timer
  "Meny",                                       // Menu
  "Inställningar",                              // Settings
  "Upptining",                                  // Defrost
  "Program",                                    // Programs
  "Klocka",                                     // Clock
  "Ställ tid",                                  // SetTime
  "Auto-tid (Wi-Fi)",                           // AutoTime
  "Ljusstyrka",                                 // Brightness
  "Dämpad nivå",                                // DimLevel
  "Dämpa efter",                                // DimDelay
  "Aldrig",                                     // Never
  "Ljud",                                       // Sound
  "Knappljud",                                  // KeyClicks
  "Volym",                                      // Volume
  "Låg",                                        // Low
  "Medel",                                      // Medium
  "Hög",                                        // High
  "På",                                         // On
  "Av",                                         // Off
  "Språk",                                      // Language
  "Kött",                                       // Meat
  "Fågel",                                      // Poultry
  "Fisk",                                       // Fish
  "Bröd",                                       // Bread
  "Värm tallrik",                               // ReheatPlate
  "Dryck",                                      // Beverage
  "Popcorn",                                    // Popcorn
  "Mjukt smör",                                 // SoftenButter
  "Smält choklad",                              // MeltChocolate
  "Favorit",                                    // Favourite
  "Spara som favorit",                          // SaveFavourite
  "Tom",                                        // Empty
  "Ställ in en tid först",                      // SetTimeFirst
  "Sparat",                                     // Saved
  "Vänd maten",                                 // TurnFood
  "Wi-Fi är av eller saknar nätverk",           // NoWifi
  "Vrid + tryck, eller peka   Stop: tillbaka",  // MenuHint
  "Vrid: ställ   Tryck: OK   Stop: tillbaka",   // EditHint
  "Vrid: ställ   Tryck: nästa   Peka: fält",    // ClockHint
  "Service",                                    // Service
  "Stop/Program/tryck: avsluta",                // ServiceHint
  "Wi-Fi",                                      // Wifi
  "Nätverk",                                    // Network
  "Inte inställt",                              // NotSet
  "Glöm nätverket",                             // ForgetNetwork
  "Nätverket glömt",                            // Forgotten
  "Säkerhetsnyckeln kunde inte skapas",         // NoKey
  "Wi-Fi-inställning",                          // WifiSetup
  "Lösenord",                                   // Password
  "Adress",                                     // Address
  "Skanna koden med telefonen",                 // SetupScan
  "Telefon ansluten: välj ditt nätverk",        // SetupPhone
  "Testar %s...",                               // SetupTesting
  "Ansluten och sparad",                        // SetupOk
  "Nätverket hittades inte",                    // SetupNotFound
  "Kunde inte ansluta: fel lösenord?",          // SetupNoConnect
  "Kunde inte spara lösenordet",                // SetupSaveFailed
  "Inställningen avbröts (tid)",                // SetupTimeout
  "Stop: avbryt",                               // SetupHint
  "Stop/tryck: tillbaka",                       // SetupDoneHint
  "Välj ditt nätverk",                          // PageChoose
  "Annat nätverk (namn)",                       // PageOther
  "Anslut",                                     // PageConnect
  "Sök igen",                                   // PageRescan
  "Söker efter nätverk...",                     // PageSearching
  "Testar anslutningen...",                     // PageTesting
  "Ansluten! Nätverket är sparat. Du kan stänga sidan.",  // PageOk
  "Försök igen",                                // PageTryAgain
  "Uppdatering",                                // FirmwareUpdate
  "Inte under tillagning",                      // NotWhileCooking
  "Öppna adressen och ange lösenordet",          // UpdateSignIn
  "På ditt nätverk: %s",                        // UpdateHome
  "Tar emot... %u %%",                          // UpdateReceiving
  "Installerad, startar om...",                 // UpdateDone
  "Uppdateringen misslyckades: %s",             // UpdateFailed
  "Stängd (ingen aktivitet)",                   // UpdateClosed
  "Version som körs",                           // PageVersion
  "Programfil (.bin)",                          // PageFile
  "Installera",                                 // PageInstall
  "Tar ca 30 s. Håll sidan öppen; ugnen startar om när den är klar.",  // PageUpdateNote
  "Installerad. Ugnen startar om nu.",          // PageUpdateOk
  "Uppdateringen misslyckades:",                // PageUpdateFailed
  "Logga in",                                   // PageSignIn
  "Fel lösenord",                               // PageWrongPassword
  "Öppnas ingen filväljare här: öppna http://192.168.4.1 i din vanliga webbläsare (Chrome, Safari).",  // PageBrowserHint
  "Felsökningslogg",                            // DebugLog
  "Loggning",                                   // Logging
  "Ladda ner",                                  // DownloadLogs
  "Inget SD-kort",                              // NoSdCard
  "Inga loggfiler än.",                         // PageNoLogs
  "Radera loggar",                              // PageDeleteLogs
  "Loggarna är raderade.",                      // PageLogsDeleted
  "Om",                                         // About
  "Programvara",                                // AboutFirmware
  "Datum",                                      // AboutDate
  "Bygge",                                      // AboutBuild
  "Ugn",                                        // AboutOven
  "Modell",                                     // AboutOvenModel
  "Typ",                                        // AboutType
  "E-nr",                                       // AboutENr
  "Serienr",                                    // AboutSerial
  "Kort",                                       // AboutBoard
  "Modell",                                     // AboutModel
  "Krets",                                      // AboutChip
  "CPU",                                        // AboutCpu
  "Flash",                                      // AboutFlash
  "PSRAM",                                      // AboutPsram
  "Skärm",                                      // AboutDisplay
  "Pekskärm",                                   // AboutTouch
  "Klockkrets",                                 // AboutClockChip
  "Reläer",                                     // AboutRelays
  "Magnetron",                                  // AboutMagnetron
  "LVGL",                                       // AboutLvgl
  "Arduino-kärna",                              // AboutCore
  "orig. relä",                                 // OriginalRelay
  "IP-adress",                                  // AboutIp
  "Senaste IP",                                 // AboutLastIp
  "MAC",                                        // AboutMac
  "Ansluten",                                   // Connected
  "Påskägg",                                    // EasterEggs
  "Slutmelodi",                                 // EggMelody
  "Upphovsmän",                                 // EggAuthors
  "Stop: tillbaka   Stäng luckan: omstart",     // EggHint
};

static const char *const GERMAN[] = {
  "OFFEN",                                      // Open
  "Ende",                                       // End
  "Pause",                                      // Paused
  "Timer",                                      // Timer
  "Menü",                                       // Menu
  "Einstellungen",                              // Settings
  "Auftauen",                                   // Defrost
  "Programme",                                  // Programs
  "Uhr",                                        // Clock
  "Uhrzeit stellen",                            // SetTime
  "Auto-Zeit (WLAN)",                           // AutoTime
  "Helligkeit",                                 // Brightness
  "Dimmstufe",                                  // DimLevel
  "Dimmen nach",                                // DimDelay
  "Nie",                                        // Never
  "Ton",                                        // Sound
  "Tastenton",                                  // KeyClicks
  "Lautstärke",                                 // Volume
  "Leise",                                      // Low
  "Mittel",                                     // Medium
  "Laut",                                       // High
  "Ein",                                        // On
  "Aus",                                        // Off
  "Sprache",                                    // Language
  "Fleisch",                                    // Meat
  "Geflügel",                                   // Poultry
  "Fisch",                                      // Fish
  "Brot",                                       // Bread
  "Aufwärmen",                                  // ReheatPlate
  "Getränk",                                    // Beverage
  "Popcorn",                                    // Popcorn
  "Butter weich",                               // SoftenButter
  "Schokolade",                                 // MeltChocolate
  "Favorit",                                    // Favourite
  "Favorit speichern",                          // SaveFavourite
  "Leer",                                       // Empty
  "Erst eine Zeit einstellen",                  // SetTimeFirst
  "Gespeichert",                                // Saved
  "Essen wenden",                               // TurnFood
  "WLAN ist aus oder ohne Netzwerk",            // NoWifi
  "Drehen + drücken, tippen   Stop: zurück",    // MenuHint
  "Drehen: Wert  Drücken: OK  Stop: zurück",    // EditHint
  "Drehen  Drücken: weiter  Tippen: Feld",      // ClockHint
  "Service",                                    // Service
  "Stop/Program/Drücken: Ende",                 // ServiceHint
  "WLAN",                                       // Wifi
  "Netzwerk",                                   // Network
  "Nicht gesetzt",                              // NotSet
  "Netzwerk vergessen",                         // ForgetNetwork
  "Netzwerk gelöscht",                          // Forgotten
  "Schlüssel nicht erstellt",                   // NoKey
  "WLAN einrichten",                            // WifiSetup
  "Passwort",                                   // Password
  "Adresse",                                    // Address
  "Code mit dem Handy scannen",                 // SetupScan
  "Handy verbunden: Netzwerk wählen",           // SetupPhone
  "Teste %s...",                                // SetupTesting
  "Verbunden und gespeichert",                  // SetupOk
  "Netzwerk nicht gefunden",                    // SetupNotFound
  "Keine Verbindung: falsches Passwort?",       // SetupNoConnect
  "Passwort nicht gespeichert",                 // SetupSaveFailed
  "Zeit abgelaufen",                            // SetupTimeout
  "Stop: abbrechen",                            // SetupHint
  "Stop/Drücken: zurück",                       // SetupDoneHint
  "Wählen Sie Ihr Netzwerk",                    // PageChoose
  "Anderes Netzwerk (Name)",                    // PageOther
  "Verbinden",                                  // PageConnect
  "Erneut suchen",                              // PageRescan
  "Suche nach Netzwerken...",                   // PageSearching
  "Verbindung wird getestet...",                // PageTesting
  "Verbunden! Das Netzwerk ist gespeichert. Sie können diese Seite schließen.",  // PageOk
  "Erneut versuchen",                           // PageTryAgain
  "Firmware-Update",                            // FirmwareUpdate
  "Nicht während des Garens",                   // NotWhileCooking
  "Adresse öffnen und Passwort eingeben",       // UpdateSignIn
  "In Ihrem Netzwerk: %s",                      // UpdateHome
  "Empfange... %u %%",                          // UpdateReceiving
  "Installiert, Neustart...",                   // UpdateDone
  "Update fehlgeschlagen: %s",                  // UpdateFailed
  "Geschlossen (keine Aktivität)",              // UpdateClosed
  "Laufende Version",                           // PageVersion
  "Firmware-Datei (.bin)",                      // PageFile
  "Installieren",                               // PageInstall
  "Dauert etwa 30 s. Lassen Sie diese Seite offen; der Ofen startet danach neu.",  // PageUpdateNote
  "Installiert. Der Ofen startet jetzt neu.",   // PageUpdateOk
  "Update fehlgeschlagen:",                     // PageUpdateFailed
  "Anmelden",                                   // PageSignIn
  "Falsches Passwort",                          // PageWrongPassword
  "Öffnet sich hier keine Dateiauswahl, öffnen Sie http://192.168.4.1 in Ihrem normalen Browser (Chrome, Safari).",  // PageBrowserHint
  "Debug-Protokoll",                            // DebugLog
  "Protokollieren",                             // Logging
  "Herunterladen",                              // DownloadLogs
  "Keine SD-Karte",                             // NoSdCard
  "Noch keine Protokolldateien.",               // PageNoLogs
  "Protokolle löschen",                         // PageDeleteLogs
  "Protokolle gelöscht.",                       // PageLogsDeleted
  "Info",                                       // About
  "Firmware",                                   // AboutFirmware
  "Datum",                                      // AboutDate
  "Build",                                      // AboutBuild
  "Ofen",                                       // AboutOven
  "Modell",                                     // AboutOvenModel
  "Typ",                                        // AboutType
  "E-Nr.",                                      // AboutENr
  "Serien-Nr.",                                 // AboutSerial
  "Platine",                                    // AboutBoard
  "Modell",                                     // AboutModel
  "Chip",                                       // AboutChip
  "CPU",                                        // AboutCpu
  "Flash",                                      // AboutFlash
  "PSRAM",                                      // AboutPsram
  "Display",                                    // AboutDisplay
  "Touch",                                      // AboutTouch
  "Uhrenchip",                                  // AboutClockChip
  "Relais",                                     // AboutRelays
  "Magnetron",                                  // AboutMagnetron
  "LVGL",                                       // AboutLvgl
  "Arduino-Core",                               // AboutCore
  "orig. Relais",                               // OriginalRelay
  "IP-Adresse",                                 // AboutIp
  "Letzte IP",                                  // AboutLastIp
  "MAC",                                        // AboutMac
  "Verbunden",                                  // Connected
  "Ostereier",                                  // EasterEggs
  "Endmelodie",                                 // EggMelody
  "Autoren",                                    // EggAuthors
  "Stop: zurück   Tür zu: Neustart",            // EggHint
};

static const char *const FRENCH[] = {
  "Ouvert",                                     // Open
  "Fin",                                        // End
  "Pause",                                      // Paused
  "Minuterie",                                  // Timer
  "Menu",                                       // Menu
  "Réglages",                                   // Settings
  "Décongeler",                                 // Defrost
  "Programmes",                                 // Programs
  "Horloge",                                    // Clock
  "Régler l'heure",                             // SetTime
  "Heure auto (Wi-Fi)",                         // AutoTime
  "Luminosité",                                 // Brightness
  "Niveau veille",                              // DimLevel
  "Veille après",                               // DimDelay
  "Jamais",                                     // Never
  "Son",                                        // Sound
  "Bip touches",                                // KeyClicks
  "Volume",                                     // Volume
  "Faible",                                     // Low
  "Moyen",                                      // Medium
  "Fort",                                       // High
  "Oui",                                        // On
  "Non",                                        // Off
  "Langue",                                     // Language
  "Viande",                                     // Meat
  "Volaille",                                   // Poultry
  "Poisson",                                    // Fish
  "Pain",                                       // Bread
  "Réchauffer",                                 // ReheatPlate
  "Boisson",                                    // Beverage
  "Pop-corn",                                   // Popcorn
  "Beurre mou",                                 // SoftenButter
  "Chocolat",                                   // MeltChocolate
  "Favori",                                     // Favourite
  "Enregistrer favori",                         // SaveFavourite
  "Vide",                                       // Empty
  "Réglez d'abord une durée",                   // SetTimeFirst
  "Enregistré",                                 // Saved
  "Retournez l'aliment",                        // TurnFood
  "Wi-Fi désactivé ou sans réseau",             // NoWifi
  "Tourner/appuyer/toucher  Stop : retour",     // MenuHint
  "Tourner : régler  Appui : OK  Stop : retour",  // EditHint
  "Tourner  Appui : suite  Toucher : champ",    // ClockHint
  "Service",                                    // Service
  "Stop/Program/appui : quitter",               // ServiceHint
  "Wi-Fi",                                      // Wifi
  "Réseau",                                     // Network
  "Non défini",                                 // NotSet
  "Oublier le réseau",                          // ForgetNetwork
  "Réseau oublié",                              // Forgotten
  "Clé de sécurité non créée",                  // NoKey
  "Réglage Wi-Fi",                              // WifiSetup
  "Mot de passe",                               // Password
  "Adresse",                                    // Address
  "Scannez le code avec votre téléphone",       // SetupScan
  "Téléphone connecté : choisissez le réseau",  // SetupPhone
  "Test de %s...",                              // SetupTesting
  "Connecté et enregistré",                     // SetupOk
  "Réseau introuvable",                         // SetupNotFound
  "Connexion impossible : mot de passe ?",      // SetupNoConnect
  "Impossible d'enregistrer le mot de passe",   // SetupSaveFailed
  "Délai dépassé",                              // SetupTimeout
  "Stop : annuler",                             // SetupHint
  "Stop/appui : retour",                        // SetupDoneHint
  "Choisissez votre réseau",                    // PageChoose
  "Autre réseau (nom)",                         // PageOther
  "Connecter",                                  // PageConnect
  "Rechercher à nouveau",                       // PageRescan
  "Recherche des réseaux...",                   // PageSearching
  "Test de la connexion...",                    // PageTesting
  "Connecté ! Le réseau est enregistré. Vous pouvez fermer cette page.",  // PageOk
  "Réessayer",                                  // PageTryAgain
  "Mise à jour",                                // FirmwareUpdate
  "Pas pendant la cuisson",                     // NotWhileCooking
  "Ouvrez l'adresse et saisissez le mot de passe",  // UpdateSignIn
  "Sur votre réseau : %s",                      // UpdateHome
  "Réception... %u %%",                         // UpdateReceiving
  "Installé, redémarrage...",                   // UpdateDone
  "Échec de la mise à jour : %s",               // UpdateFailed
  "Fermé (aucune activité)",                    // UpdateClosed
  "Version actuelle",                           // PageVersion
  "Fichier du micrologiciel (.bin)",            // PageFile
  "Installer",                                  // PageInstall
  "Environ 30 s. Gardez cette page ouverte ; le four redémarre ensuite.",  // PageUpdateNote
  "Installé. Le four redémarre maintenant.",    // PageUpdateOk
  "Échec de la mise à jour :",                  // PageUpdateFailed
  "Se connecter",                               // PageSignIn
  "Mot de passe incorrect",                     // PageWrongPassword
  "Si aucun sélecteur de fichier ne s'ouvre ici, ouvrez http://192.168.4.1 dans votre navigateur habituel (Chrome, Safari).",  // PageBrowserHint
  "Journal debug",                              // DebugLog
  "Journalisation",                             // Logging
  "Télécharger",                                // DownloadLogs
  "Pas de carte SD",                            // NoSdCard
  "Aucun fichier journal pour l'instant.",      // PageNoLogs
  "Supprimer les journaux",                     // PageDeleteLogs
  "Journaux supprimés.",                        // PageLogsDeleted
  "À propos",                                   // About
  "Micrologiciel",                              // AboutFirmware
  "Date",                                       // AboutDate
  "Build",                                      // AboutBuild
  "Four",                                       // AboutOven
  "Modèle",                                     // AboutOvenModel
  "Type",                                       // AboutType
  "N° E",                                       // AboutENr
  "N° de série",                                // AboutSerial
  "Carte",                                      // AboutBoard
  "Modèle",                                     // AboutModel
  "Puce",                                       // AboutChip
  "CPU",                                        // AboutCpu
  "Flash",                                      // AboutFlash
  "PSRAM",                                      // AboutPsram
  "Écran",                                      // AboutDisplay
  "Tactile",                                    // AboutTouch
  "Puce horloge",                               // AboutClockChip
  "Relais",                                     // AboutRelays
  "Magnétron",                                  // AboutMagnetron
  "LVGL",                                       // AboutLvgl
  "Noyau Arduino",                              // AboutCore
  "relais orig.",                               // OriginalRelay
  "Adresse IP",                                 // AboutIp
  "Dernière IP",                                // AboutLastIp
  "MAC",                                        // AboutMac
  "Connecté",                                   // Connected
  "Œufs de Pâques",                             // EasterEggs
  "Mélodie de fin",                             // EggMelody
  "Auteurs",                                    // EggAuthors
  "Stop : retour   Fermer : redémarrer",        // EggHint
};

static const char *const SPANISH[] = {
  "Abierto",                                    // Open
  "Fin",                                        // End
  "Pausa",                                      // Paused
  "Temporizador",                               // Timer
  "Menú",                                       // Menu
  "Ajustes",                                    // Settings
  "Descongelar",                                // Defrost
  "Programas",                                  // Programs
  "Reloj",                                      // Clock
  "Poner la hora",                              // SetTime
  "Hora auto (Wi-Fi)",                          // AutoTime
  "Brillo",                                     // Brightness
  "Nivel atenuado",                             // DimLevel
  "Atenuar tras",                               // DimDelay
  "Nunca",                                      // Never
  "Sonido",                                     // Sound
  "Clic de teclas",                             // KeyClicks
  "Volumen",                                    // Volume
  "Bajo",                                       // Low
  "Medio",                                      // Medium
  "Alto",                                       // High
  "Sí",                                         // On
  "No",                                         // Off
  "Idioma",                                     // Language
  "Carne",                                      // Meat
  "Aves",                                       // Poultry
  "Pescado",                                    // Fish
  "Pan",                                        // Bread
  "Recalentar",                                 // ReheatPlate
  "Bebida",                                     // Beverage
  "Palomitas",                                  // Popcorn
  "Mantequilla",                                // SoftenButter
  "Chocolate",                                  // MeltChocolate
  "Favorito",                                   // Favourite
  "Guardar favorito",                           // SaveFavourite
  "Vacío",                                      // Empty
  "Primero ajuste un tiempo",                   // SetTimeFirst
  "Guardado",                                   // Saved
  "Dé la vuelta a la comida",                   // TurnFood
  "Wi-Fi apagado o sin red",                    // NoWifi
  "Girar + pulsar, o tocar   Stop: atrás",      // MenuHint
  "Girar: ajustar  Pulsar: OK  Stop: atrás",    // EditHint
  "Girar: ajustar  Pulsar: sig.  Tocar: campo",  // ClockHint
  "Servicio",                                   // Service
  "Stop/Program/pulsar: salir",                 // ServiceHint
  "Wi-Fi",                                      // Wifi
  "Red",                                        // Network
  "Sin configurar",                             // NotSet
  "Olvidar la red",                             // ForgetNetwork
  "Red olvidada",                               // Forgotten
  "No se pudo crear la clave",                  // NoKey
  "Configurar Wi-Fi",                           // WifiSetup
  "Contraseña",                                 // Password
  "Dirección",                                  // Address
  "Escanee el código con el móvil",             // SetupScan
  "Móvil conectado: elija su red",              // SetupPhone
  "Probando %s...",                             // SetupTesting
  "Conectado y guardado",                       // SetupOk
  "Red no encontrada",                          // SetupNotFound
  "No conecta: ¿contraseña incorrecta?",        // SetupNoConnect
  "No se pudo guardar la contraseña",           // SetupSaveFailed
  "Tiempo agotado",                             // SetupTimeout
  "Stop: cancelar",                             // SetupHint
  "Stop/pulsar: atrás",                         // SetupDoneHint
  "Elija su red",                               // PageChoose
  "Otra red (nombre)",                          // PageOther
  "Conectar",                                   // PageConnect
  "Buscar de nuevo",                            // PageRescan
  "Buscando redes...",                          // PageSearching
  "Probando la conexión...",                    // PageTesting
  "¡Conectado! La red está guardada. Puede cerrar esta página.",  // PageOk
  "Reintentar",                                 // PageTryAgain
  "Actualización",                              // FirmwareUpdate
  "No mientras cocina",                         // NotWhileCooking
  "Abra la dirección e introduzca la contraseña",  // UpdateSignIn
  "En su red: %s",                              // UpdateHome
  "Recibiendo... %u %%",                        // UpdateReceiving
  "Instalado, reiniciando...",                  // UpdateDone
  "Error de actualización: %s",                 // UpdateFailed
  "Cerrado (sin actividad)",                    // UpdateClosed
  "Versión actual",                             // PageVersion
  "Archivo de firmware (.bin)",                 // PageFile
  "Instalar",                                   // PageInstall
  "Tarda unos 30 s. Mantenga esta página abierta; el horno se reinicia al terminar.",  // PageUpdateNote
  "Instalado. El horno se reinicia ahora.",     // PageUpdateOk
  "Error de actualización:",                    // PageUpdateFailed
  "Entrar",                                     // PageSignIn
  "Contraseña incorrecta",                      // PageWrongPassword
  "Si aquí no se abre el selector de archivos, abra http://192.168.4.1 en su navegador habitual (Chrome, Safari).",  // PageBrowserHint
  "Registro debug",                             // DebugLog
  "Registrar",                                  // Logging
  "Descargar",                                  // DownloadLogs
  "Sin tarjeta SD",                             // NoSdCard
  "Aún no hay archivos de registro.",           // PageNoLogs
  "Borrar registros",                           // PageDeleteLogs
  "Registros borrados.",                        // PageLogsDeleted
  "Acerca de",                                  // About
  "Firmware",                                   // AboutFirmware
  "Fecha",                                      // AboutDate
  "Build",                                      // AboutBuild
  "Horno",                                      // AboutOven
  "Modelo",                                     // AboutOvenModel
  "Tipo",                                       // AboutType
  "N.º E",                                      // AboutENr
  "N.º de serie",                               // AboutSerial
  "Placa",                                      // AboutBoard
  "Modelo",                                     // AboutModel
  "Chip",                                       // AboutChip
  "CPU",                                        // AboutCpu
  "Flash",                                      // AboutFlash
  "PSRAM",                                      // AboutPsram
  "Pantalla",                                   // AboutDisplay
  "Táctil",                                     // AboutTouch
  "Chip de reloj",                              // AboutClockChip
  "Relés",                                      // AboutRelays
  "Magnetrón",                                  // AboutMagnetron
  "LVGL",                                       // AboutLvgl
  "Núcleo Arduino",                             // AboutCore
  "relé orig.",                                 // OriginalRelay
  "Dir. IP",                                    // AboutIp
  "Última IP",                                  // AboutLastIp
  "MAC",                                        // AboutMac
  "Conectado",                                  // Connected
  "Huevos de Pascua",                           // EasterEggs
  "Melodía final",                              // EggMelody
  "Autores",                                    // EggAuthors
  "Stop: atrás   Cerrar puerta: reinicio",      // EggHint
};

static_assert(sizeof(ENGLISH) / sizeof(ENGLISH[0]) == (size_t)Str::Count, "ENGLISH table must match Str");
static_assert(sizeof(SWEDISH) / sizeof(SWEDISH[0]) == (size_t)Str::Count, "SWEDISH table must match Str");
static_assert(sizeof(GERMAN) / sizeof(GERMAN[0]) == (size_t)Str::Count, "GERMAN table must match Str");
static_assert(sizeof(FRENCH) / sizeof(FRENCH[0]) == (size_t)Str::Count, "FRENCH table must match Str");
static_assert(sizeof(SPANISH) / sizeof(SPANISH[0]) == (size_t)Str::Count, "SPANISH table must match Str");

// Indexed by Language.
static const char *const *const TABLES[] = { ENGLISH, SWEDISH, GERMAN, FRENCH, SPANISH };
static const char *const NAMES[] = { "English", "Svenska", "Deutsch", "Français", "Español" };
static_assert(sizeof(TABLES) / sizeof(TABLES[0]) == (size_t)Language::Count, "TABLES must match Language");
static_assert(sizeof(NAMES) / sizeof(NAMES[0]) == (size_t)Language::Count, "NAMES must match Language");

const char *tr(Str s) {
  return TABLES[(size_t)settings.language][(size_t)s];
}

const char *languageName(uint8_t language) {
  return language < (uint8_t)Language::Count ? NAMES[language] : "?";
}

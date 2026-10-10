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
  "No encryption key (serial: keygen)",         // NoKey
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
  "Mjuka upp smör",                             // SoftenButter
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
  "Ingen krypteringsnyckel (serie: keygen)",    // NoKey
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
  "Uppdatera programvara",                      // FirmwareUpdate
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
};

static_assert(sizeof(ENGLISH) / sizeof(ENGLISH[0]) == (size_t)Str::Count, "ENGLISH table must match Str");
static_assert(sizeof(SWEDISH) / sizeof(SWEDISH[0]) == (size_t)Str::Count, "SWEDISH table must match Str");

const char *tr(Str s) {
  const char *const *table = settings.language == Language::Swedish ? SWEDISH : ENGLISH;
  return table[(size_t)s];
}

const char *languageName(uint8_t language) {
  return language == (uint8_t)Language::Swedish ? "Svenska" : "English";
}

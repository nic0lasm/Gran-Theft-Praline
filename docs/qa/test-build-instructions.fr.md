# Ouvrir la build de test (macOS et Windows)

*Version anglaise : [test-build-instructions.md](test-build-instructions.md).*

La build de test n'est pas signée par un compte développeur payant : les
deux systèmes avertissent la première fois. C'est normal. N'ouvrez que des
archives que Nico vous a envoyées directement.

## 1. Télécharger et vérifier

Vous recevez un ou plusieurs fichiers
`GranTheftPraline-<version>-<système>[-partNsurM].zip` et un fichier
`SHA256SUMS`. S'il y a plusieurs parties, extrayez-les toutes **dans le même
dossier**.

- macOS (Terminal, dans le dossier de téléchargement) :
  `shasum -a 256 -c SHA256SUMS`
- Windows (PowerShell, dans le dossier de téléchargement) :
  `Get-FileHash .\*.zip -Algorithm SHA256` et comparer à `SHA256SUMS`.

Chaque ligne doit correspondre. Sinon, télécharger de nouveau.

## 2. macOS (Gatekeeper)

1. Double-cliquer le zip, puis déplacer `GranTheftPraline.app` dans
   Applications ou un autre dossier.
2. Double-cliquer dessus. macOS dit qu'il ne peut pas vérifier l'app :
   cliquer **Terminé**.
3. Ouvrir **Réglages Système → Confidentialité et sécurité**, descendre
   jusqu'au message sur GranTheftPraline, cliquer **Ouvrir quand même**, puis
   confirmer. (Sur un macOS plus ancien : clic droit sur l'app, **Ouvrir**,
   **Ouvrir**.)
4. Si l'app reste bloquée, lancer une fois dans Terminal :
   `xattr -dr com.apple.quarantine /chemin/vers/GranTheftPraline.app`.

Prérequis : Mac Apple silicon, 16 Go de mémoire conseillés, environ 4 Go de
disque. Le premier lancement compile des shaders et peut durer une minute.

## 3. Windows (SmartScreen)

1. Extraire le ou les zip (clic droit, **Extraire tout**) dans un dossier
   comme `C:\Games\GranTheftPraline`.
2. Double-cliquer `GranTheftPraline.exe`. Si **Windows a protégé votre PC**
   s'affiche : cliquer **Informations complémentaires**, puis **Exécuter
   quand même**.
3. Si un antivirus met le fichier en quarantaine, prévenir Nico ; ne pas le
   désactiver.

Prérequis : Windows 10 ou 11 64 bits, carte graphique 8 Go, 16 Go de
mémoire, environ 6 Go de disque, pilote graphique à jour.

## 4. Où sont les journaux et les rapports de plantage

La build de test écrit tout en local et n'envoie rien.

| Système | Dossier |
| ---- | ---- |
| macOS | `~/Library/Application Support/Epic/GranTheftPraline/Saved/` (`Logs`, `Crashes`, `Profiling`) |
| Windows | `%LOCALAPPDATA%\GranTheftPraline\Saved\` (`Logs`, `Crashes`, `Profiling`) |

Appuyer sur **F9** en jeu exporte temps d'image, position et version dans le
dossier `Exports` à côté (voir [test-build-spec.fr.md](test-build-spec.fr.md)).
Pour signaler un bogue, compresser ce dossier et le joindre.

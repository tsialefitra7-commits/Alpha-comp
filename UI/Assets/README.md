# Assets — alpha-comp UI

Ce dossier est prévu pour les ressources visuelles (icônes, polices
personnalisées). Volontairement, **aucun binaire n'est versionné** :

- Les polices utilisées sont celles du système via JUCE (`juce::Font`),
  comme le fait le Pro-C 2 (polices fines système, pas de fonte embarquée).
- Les icônes / logos sont dessinés en code (`ProC2Theme`, `CompressionCurve`,
  `GainReductionMeter`) : aucun fichier image n'est nécessaire.

Si vous ajoutez un jour une police ou une image, placez-la dans
`Fonts/` ou `Images/` et chargez-la via `juce_add_binary_data` dans
`UI/CMakeLists.txt` (voir la documentation JUCE).

# esp12e_linky_conso_day

Le dispositif se compose de 2 modules :

- module de détection et de comptage de l'allumage du voyant LED sur le compteur LINKY.

  Ce module utile une photo-transistor pour détecter l'allumage de la LED ( chaque fois 1 watt est consommé)

  Ensuite il envoie vers un broker MQTT les topics comme intervalle entre 2 allumages, conso intantanée en kWh et
  le cumul des watts.

- module d'affichage du cumul de watts consommé depuis minuit.

  Ce module est un ESP12E avec affichage OLED qui va permettre de recevoir du broker MQTT le topic "cumul de watts" et ainsi d'afficher la valeur.

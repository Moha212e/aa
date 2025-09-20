# Protocole de Login Patient

Client -> Serveur (connexion courte, une requête) :

Nouveau patient:
  LOGIN_NEW;LASTNAME;FIRSTNAME

Patient existant:
  LOGIN_EXIST;ID;LASTNAME;FIRSTNAME

Réponses serveur:
  LOGIN_OK;ID              (succès, ID attribué ou confirmé)
  LOGIN_FAIL;REASON        (échec)

Reasons possibles actuelles:
  DB           : problème connexion BD
  FORMAT       : format message invalide
  INSERT       : insertion patient impossible
  NOT_FOUND    : patient inconnu
  UNKNOWN_CMD  : commande non reconnue

Le serveur insère un patient nouveau avec une date de naissance fictive (2000-01-01) à améliorer.

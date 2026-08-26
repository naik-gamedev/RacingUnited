# CLOUDURP15BK1 – Validation Hotfix

CLOUDURP15BK deliberately reduced cloud-shadow and cloud-light sampling as part of the Sun performance overhaul. The existing architecture validator still required the old exact 15-sample shadow loop string and therefore rejected the intentional optimization before compilation. BK1 updates that safety-net to accept BK's explicit 8-sample cloud-shadow / one-step cloud-light variant while preserving the rest of the translated UnityVolumetricCloudsURP architecture checks.

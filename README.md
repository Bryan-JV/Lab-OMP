

**C4G330-Bryan Jiménez Vindas-Lab.OMP**

## Máquina y metodología

Las mediciones se realizaron en una maquina Linux con 2 núcleos físicos, 2 hilos por núcleo con HyperThreading. Cada valor es la mediana de 5 corridas. Las configuraciones se ejecutaron intercaladas.


## Mediciones

### Tabla 1 — pipeline completo, 4000 × 3000 

| hilos |  gray |  blur | sobel |  hist | total | speedup |
|-------|-------|-------|-------|-------|-------|---------|
|   1   | 0.052 | 0.420 | 0.101 | 0.012 | 0.742 |  1.00x  |
|   2   | 0.027 | 0.217 | 0.057 | 0.006 | 0.459 |  1.62x  |
|   4   | 0.025 | 0.188 | 0.060 | 0.006 | 0.441 |  1.68x  |
|   8   | 0.025 | 0.185 | 0.055 | 0.007 | 0.425 |  1.75x  |

El speedup es respecto a pipeline_omp con 1 hilo.

### Tabla 2 — etapa 4

| hilos | naive | critical | atomic | reduction |
|-------|-------|----------|--------|-----------|
|   1   | 0.011 |  0.323   |  0.112 |   0.012   |
|   2   | 0.087 |  0.572   |  0.181 |   0.006   |
|   4   | 0.065 |  0.868   |  0.204 |   0.006   |
|   8   | 0.068 |  0.888   |  0.202 |   0.007   |

Conteos perdidos por naive en cada una de las 5 corridas:

| hilos |                   conteos perdidos                        |
|-------|-----------------------------------------------------------|
|   1   | 0, 0, 0, 0, 0                                             |
|   2   | 4 825 352 · 4 507 086 · 4 808 211 · 4 640 964 · 4 572 813 |
|   4   | 6 835 298 · 6 476 658 · 5 797 096 · 6 596 242 · 5 041 500 |
|   8   | 6 461 718 · 6 507 754 · 5 515 477 · 6 674 652 · 5 559 701 |


## Respuestas

**1. Naive con uno y con cuatro hilos**

Con un solo hilo no hay otro hilo que esté modificando el histograma al mismo tiempo, entonces cada hist[b]++ se termina de leer, sumar y escribir antes de pasar al siguiente, por eso no se pierde ningún conteo. Con cuatro hilos esas tres operaciones se pueden mezclar entre hilos. Si dos hilos leen el mismo bin al mismo tiempo, ambos escriben el mismo valor + 1 y uno de los incrementos se pierde, lo cual es una race condition. La cantidad de conteos perdidos cambia en cada corrida porque depende de cómo el sistema operativo vaya alternando los hilos y eso nunca pasa igual dos veces.

**2. ¿Dónde está la barrera?**

La barrera está implícita al final de cada #pragma omp parallel for. Cuando termina la región paralela todos los hilos se sincronizan y el hilo principal no continúa hasta que todos hayan terminado sus iteraciones. Como cada etapa está en su propia función con su propia región paralela no fue necesario escribir #pragma omp barrier.

**3. critical contra atomic**

Atomic es más rápido que el critial, con 1 hilo tarda 0.112s contra 0.323s de critical y con 4 hilos 0.204s contra 0.868s. Esto pasa porque critical funciona como un lock global, cada incremento tiene que pedir y soltar ese lock y mientras un hilo lo tiene ningún otro puede modificar ningún bin aunque sea otro distinto. En cambio atomic se convierte en una sola instrucción atómica del procesador sobre ese bin específico, entonces no hay que llamar al runtime de OpenMP y dos hilos que actualizan bins diferentes pueden hacerlo al mismo tiempo.

**4. critical al agregar hilos**

Con critical el tiempo pasa de 0.323s con 1 hilo a 0.572 s con 2 y 0.868s con 4, o sea que en vez de mejorar empeora. Como todo el incremento está dentro de la sección crítica, solo un hilo puede trabajar a la vez y el trabajo sigue siendo serial. Además cada hilo que se agrega es uno más esperando por el mismo lock, lo que aumenta el tiempo que se pierde sincronizando. Esto implica que no conviene poner un critical dentro de un loop que se ejecuta muchisimas veces, en estos casos es mejor que cada hilo tenga su propio acumulador y se combinen al final, que es lo que hace reduction (0.006s con 4 hilos).

**5. Amdahl**

Con 1 hilo el programa mide P = 0.789, o sea que la parte serial (load + save) es 1 - P = 0.211, y el techo de Amdahl es 1 / 0.211 = 4.7x.

|    N    |   2  |   4  |   8  |   ∞  |
|---------|------|------|------|------|
| Teórico | 1.65 | 2.45 | 3.23 | 4.74 |
| Medido  | 1.62 | 1.68 | 1.75 |   -  | 

Con 2 hilos lo medido coincide casi exacto con la teoría, pero con 4 y 8 no, ya que la máquina solo tiene 2 núcleos físicos y el HyperThreading aporta muy poco. Pasar de 8 a 16 núcleos solo subiría el speedup de 3.23x a 3.84x, por lo que no valdría la pena tener más de unos 8 núcleos para este pipeline. Para subir el techo lo que sirve es hacer más rápido el save, porque agregar núcleos no cambia la parte serial. El save es 0.134s de los 0.156s seriales, si se hiciera el doble de rápido la parte serial quedaría en 0.022 + 0.067 = 0.089s  y el techo subiría de 4.7x a 7.6x.

**6. Granularidad (400 x 300)**

Mediana de 7 corridas:

| serial | omp 1 | omp 2 | omp 4 | omp 8 |
|--------|-------|-------|-------|-------|
|  0.007 | 0.008 | 0.006 | 0.016 | 0.006 | Total |

Con la imagen pequeña OpenMP ya no gana. Con 2 y 8 hilos se ahorra como mucho 1 ms y con 4 hilos es incluso más del doble de lento que el serial. Como la imagen solo tiene 120 000 píxeles, cada etapa dura 1 ms o menos, que es más o menos lo mismo que cuesta crear los hilos, repartir el trabajo y sincronizarlos al final. Esto quiere decir que se está por debajo del crossover point, donde el costo de OpenMP es mayor que lo que se gana paralelizando. Con la imagen de 4000 x 3000 cada etapa dura cientos de milisegundos y ese costo extra casi no se nota.

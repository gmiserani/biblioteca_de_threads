# PAGINADOR DE MEMÓRIA - RELATÓRIO

1. Termo de compromisso

Os membros do grupo afirmam que todo o código desenvolvido para este
trabalho é de autoria própria.  Exceto pelo material listado no item
3 deste relatório, os membros do grupo afirmam não ter copiado
material da Internet nem ter obtido código de terceiros.

2. Membros do grupo e alocação de esforço

Preencha as linhas abaixo com o nome e o e-mail dos integrantes do
grupo.  Substitua marcadores `XX` pela contribuição de cada membro
do grupo no desenvolvimento do trabalho (os valores devem somar
100%).

  * Gabriela Moraes Miserani de Freitas <gabrielamiserani@gmail.com> 50%
  * Raissa Miranda Maciel  <raissamm01@hotmail.com> 50%

3. Referências bibliográficas
https://www.ibm.com/docs/en/zos/2.1.0?topic=functions-sigaction-examine-change-signal-action
https://bitismyth.wordpress.com/2011/03/22/dica-sinais/
https://forum.lazarus.freepascal.org/index.php?topic=48694.0

4. Estruturas de dados

  1. Descreva e justifique as estruturas de dados utilizadas para
     gerência das threads de espaço do usuário (partes 1, 2 e 5).

    O código apresentado é uma implementação de threads em espaço de usuário que utiliza as seguintes estruturas de dados:

    A estrutura de dados dccthread_t é utilizada para representar cada thread, armazenando seu contexto, seu nome e a thread que está esperando (caso haja alguma). As duas listas ready_list e sleeping_list são utilizadas para armazenar as threads que estão prontas para serem executadas e as threads que estão dormindo, respectivamente.

    A lista ready_list é utilizada pelo escalonador para decidir qual thread deve ser executada em seguida. Quando uma nova thread é criada, ela é adicionada na ready_list. Quando uma thread faz uma chamada para dccthread_yield, ela é adicionada no final da ready_list, e a próxima thread a ser executada é retirada do início dessa lista. Já a lista sleeping_list é utilizada para armazenar as threads que estão em espera. Quando uma thread faz uma chamada para dccthread_sleep, ela é adicionada na sleeping_list. O escalonador periodicamente verifica se alguma thread da sleeping_list pode ser acordada. Quando uma thread é acordada, ela é adicionada na ready_list.

    A estrutura de dados manager é utilizada para representar o contexto do escalonador. A função dccthread_init cria uma thread com esse contexto, que é responsável por chamar as outras threads.

    Os timers timerp e sleep_timer são dois timers que são utilizados para gerenciar a preempção e o tempo de espera das threads. O timer timerp é utilizado para gerenciar a preempção das threads. O timer sleep_timer é utilizado para gerenciar o tempo de espera das threads que estão dormindo na sleeping_list.

    Por fim, as máscaras de sinais mask e sleepmask são utilizadas para bloquear e desbloquear sinais do sistema. Isso é necessário para garantir que as threads não sejam interrompidas em momentos inesperados.

  2. Descreva o mecanismo utilizado para sincronizar chamadas de
     dccthread_yield e disparos do temporizador (parte 4).

      Dccthread_yield e disparos do temporizador são dois mecanismos diferentes usados em sistemas operacionais para gerenciar a execução de processos concorrentes.

      Dccthread_yield é uma chamada de sistema que solicita que o sistema operacional pause a execução do processo atual e permita que outro processo execute. Isso permite que o SO execute vários processos simultaneamente, aumentando a eficiência do sistema como um todo. Quando dccthread_yield é chamado, o processo atual é adicionado a uma fila de processos prontos, e o próximo processo da fila é selecionado para executar.

      Disparos do temporizador são um mecanismo usado para agendar eventos futuros. O sistema operacional define um temporizador interno que é usado para gerar interrupções em intervalos regulares. Quando o temporizador dispara, o SO executa uma rotina de tratamento de interrupção que pode executar tarefas específicas, como atualizar o relógio do sistema, verificar se um processo está inativo por muito tempo ou executar outras tarefas agendadas.

      Para sincronizar chamadas de dccthread_yield e disparos do temporizador, o sistema operacional usa uma lista de processos prontos, que é gerenciada pelo escalonador de processos. O escalonador é responsável por decidir qual processo deve ser executado em seguida. Quando um processo chama dccthread_yield, ele é adicionado ao final da lista de processos prontos e o próximo processo da lista é selecionado para executar.Enquanto isso, o temporizador interno do sistema operacional continua a contar. Quando o temporizador dispara, o sistema operacional executa a rotina de tratamento de interrupção associada e, em seguida, verifica se algum processo está aguardando para ser executado. Se houver, o escalonador seleciona o próximo processo da lista de processos prontos e o executa.

      Dessa forma, o sistema operacional consegue equilibrar a execução de múltiplos processos concorrentes, garantindo que cada processo receba uma parcela justa do tempo de CPU disponível, ao mesmo tempo em que realiza tarefas agendadas pelo temporizador.

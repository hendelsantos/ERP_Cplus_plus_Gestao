# Diagnóstico e recuperação local

O diagnóstico está em **Configurações → Empresa e módulos → Diagnóstico**, restrito a administradores. Exibe versão do aplicativo, Qt, esquema, caminho do banco, pasta dos logs, falha de gravação dos logs e detecção de encerramento inesperado.

## Logs

Os arquivos ficam em `logs/operations.log` dentro da pasta de dados, com três arquivos anteriores (`.1` a `.3`). Cada arquivo tem limite de 1 MiB. Os eventos são gravados em UTC, com nível, componente e código fixo. Há registro de inicialização/encerramento, migração, integridade, backup/restauração e falhas de início ou rollback de transações.

O nível mínimo é configurável: Informação (padrão), Aviso ou Erro. A configuração fica em `diagnostics.ini`, fora do SQLite e dos backups. O logger aceita apenas eventos e componentes definidos no código: não recebe mensagens SQL, consultas, parâmetros, senhas, hashes, tokens, códigos de recuperação ou conteúdo dos cadastros. Mensagens do Qt no terminal não são capturadas nesses arquivos.

Falhas na gravação ou rotação são indicadas no diagnóstico e não interrompem vendas. Quando a falha ocorre durante uma sessão com logger ativo, após corrigir espaço/permissões o próximo evento tenta gravar novamente. Se a preparação inicial dos logs ou do marcador falhar, corrija o problema e reinicie o aplicativo. A rotação interrompe a tentativa em caso de falha, sem truncar o arquivo atual. Logs são locais; não há envio automático de informações para terceiros.

## Inicialização e interrupções

Depois de adquirir o bloqueio de instância, o aplicativo cria `session.active`. No encerramento normal, após finalizar os serviços e aguardar backup em andamento, remove esse marcador. Se ele existir na próxima inicialização, registra `unclean_shutdown` e informa isso no diagnóstico. Isso indica interrupção, não prova perda de dados.

Antes das migrações, a abertura do SQLite verifica `integrity_check` e `foreign_key_check`. A recuperação de transações interrompidas é feita pelo próprio SQLite ao abrir o banco. Se a verificação falhar, o aplicativo não entra na operação normal nem aplica migrações: apresenta uma janela de erro com orientação para preservar o banco e procurar recuperação por backup. Não apaga, recria nem restaura automaticamente o banco do cliente.

Se a pasta estiver indisponível ou outra instância estiver aberta, também apresenta orientação em uma janela. O esquema permanece 20.

## Validação e limites

Os testes usam diretórios temporários e verificam rotação, níveis persistidos, permissões administrativas, destino de log indisponível e recuperação da gravação, concorrência, corrupção, vínculos inválidos, rollback de migração e ausência de informações sensíveis em falhas de cadastro.

Um processo auxiliar é encerrado sem executar destrutores durante uma transação SQLite. A reabertura verifica que dados confirmados permanecem e a alteração incompleta não foi persistida, além de detectar a sessão interrompida. Esse teste não simula corte físico de energia, defeito de hardware ou disco fisicamente cheio. Windows e implantação em máquina limpa precisam de validação própria.

A verificação integral pode aumentar o tempo de abertura em bases grandes; ainda falta medir esse tempo em bases representativas de clientes. Logs ajudam a identificar componente e tipo de evento; não substituem auditoria de operações nem backup.

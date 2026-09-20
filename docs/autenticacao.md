# Autenticação local — implementação inicial

O SQLite guarda usuários e permissões; a sessão existe somente na memória do processo. Não há dependência de servidor para login. Autorização é aplicada nos serviços C++ antes das operações. Leituras requerem sessão; todos os perfis autenticados podem consultar os dados operacionais nesta primeira versão.

## Credenciais

- OpenSSL Crypto: PBKDF2-HMAC-SHA256, 600.000 iterações, salt de 16 bytes via RAND_bytes, hash de 32 bytes. Cada senha nova usa outro salt; comparação com CRYPTO_memcmp.
- Senhas de 12–128 caracteres, sem truncamento ou remoção de espaços antes de derivar o hash. Login normalizado para minúsculas.
- Cinco falhas bloqueiam a conta por 300 segundos; contador e prazo persistidos. Recuperação por código remove o bloqueio.
- Sessão contém identificador, versão e caminho do banco. Cada autorização verifica novamente usuário ativo e versão. Alterações administrativas revogam sessões.
- Setup usa transação de escrita e só cria administrador quando a tabela de usuários está vazia. Não há conta ou senha padrão.
- Troca da própria senha exige sessão ativa, senha atual correta e nova senha válida e diferente. Gera novo salt e hash em transação; a sessão atual é mantida, as demais são invalidadas e o código de recuperação existente permanece válido. Senha atual incorreta conta como tentativa falha: cinco bloqueiam por 300 segundos.

Referências: [OWASP — armazenamento de senhas](https://cheatsheetseries.owasp.org/cheatsheets/Password_Storage_Cheat_Sheet.html), [OpenSSL PBKDF2](https://docs.openssl.org/3.4/man3/PKCS5_PBKDF2_HMAC/), [OpenSSL RAND_bytes](https://docs.openssl.org/3.4/man3/RAND_bytes/), [comparação constante](https://docs.openssl.org/3.4/man3/CRYPTO_memcmp/).

## Recuperação

O primeiro administrador recebe código aleatório de 32 bytes, mostrado uma única vez. SHA-256 do código é armazenado, sem o código original. Recuperar exige login e código; gera novo salt, senha e código em transação e invalida o código anterior. A recuperação aceita qualquer administrador ativo com código válido.

Administradores adicionais recebem código por outro administrador, no botão **Código de recuperação** de **Usuários**: o código é gerado em transação, exibido uma única vez ao emissor e invalida o anterior da conta. Emitir para a própria conta, operador ou conta inativa é rejeitado; o emissor deve entregar o código por canal seguro. Editar a conta pela administração invalida seu código. Sem código nem outro administrador, não existe bypass de recuperação fornecido pelo aplicativo.

## Permissões e rastreabilidade

A matriz de permissões por ação é centralizada em `Auth::permissions()` (`core/auth`), com identificadores estáveis por ação, distintos de telas. `Auth::allowed` consulta a matriz; identificadores desconhecidos e sessões ausentes são sempre negados. Perfis são fixos, ainda não há matriz customizável.

| Identificador | Ação | Administrador | Operador |
| --- | --- | --- | --- |
| `read` | Consultar cadastros, estoque, vendas, clientes, caixa e painel | Sim | Sim |
| `catalog` | Criar, editar e ativar/inativar produtos, categorias e clientes | Sim | Não |
| `inventory` | Movimentar estoque manualmente | Sim | Não |
| `cash` | Abrir/fechar caixa, suprimento e sangria | Sim | Sim |
| `pos` | Operar o PDV e finalizar vendas | Sim | Sim |
| `settings` | Configurar empresa e módulos | Sim | Não |
| `backup` | Criar e restaurar backups | Sim | Não |
| `users` | Gerenciar usuários e emitir código de recuperação | Sim | Não |
| `audit` | Consultar o registro de auditoria de alterações administrativas | Sim | Não |

Alterações administrativas são auditadas na mesma transação em que ocorrem: criação/edição/inativação de usuários, emissão de código de recuperação, troca da própria senha e alterações de empresa/módulos. Cada registro guarda o responsável (ID e nome da ocasião), a ação, o alvo, detalhes que reconstroem a alteração e data/hora local — nunca senhas, hashes ou códigos. Falha na auditoria desfaz a alteração. A consulta, paginada, fica em **Auditoria** e é restrita a administradores. Setup inicial e recuperação por código não são auditados por ocorrerem sem sessão. Auditoria de cadastros (produtos, clientes, estoque) permanece pendente.

IDs e nomes autenticados são gravados em vendas, estoque, caixa e fechamento; isso não constitui um log completo de auditoria de cadastros e administração.

O esquema 7 acrescenta a auditoria administrativa. Recuperação de backup encerra sessão; contas, credenciais e auditoria voltam ao estado do backup. Restauração direta aceita somente esquema idêntico na versão 8.

## Limites e próximas melhorias

- Controle de acesso dentro do aplicativo não protege contra alguém com permissão do sistema operacional para editar ou copiar o SQLite. Banco e backup não estão criptografados.
- Relógio local controla bloqueio temporário. Um backup antigo pode restaurar senhas/códigos e contadores antigos; proteja os arquivos.
- Permissões customizáveis e auditoria de cadastros (produtos, clientes, estoque) ainda pendentes.
- OpenSSL deve acompanhar o pacote Windows; instalação e renderização em Windows ainda precisam ser validadas.

A permissão `pos.adjust` autoriza aplicar e finalizar vendas com desconto/acréscimo. Apenas administradores a possuem na matriz atual; operadores continuam autorizados a vendas sem ajustes.

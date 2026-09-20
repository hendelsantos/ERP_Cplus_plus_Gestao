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

Administrador: consultas, cadastros, estoque manual, caixa, PDV, configurações, backup e usuários. Operador: consultas e operações de caixa/PDV habilitados. Perfis são fixos, ainda não há matriz customizável. IDs e nomes autenticados são gravados em vendas, estoque, caixa e fechamento; isso não constitui um log completo de auditoria de cadastros e administração.

O esquema 6 preserva registros antigos com vínculos nulos. Recuperação de backup encerra sessão; contas e credenciais voltam ao estado do backup. Restauração direta aceita somente esquema idêntico na versão 6.

## Limites e próximas melhorias

- Controle de acesso dentro do aplicativo não protege contra alguém com permissão do sistema operacional para editar ou copiar o SQLite. Banco e backup não estão criptografados.
- Relógio local controla bloqueio temporário. Um backup antigo pode restaurar senhas/códigos e contadores antigos; proteja os arquivos.
- Permissões customizáveis e auditoria geral ainda pendentes.
- OpenSSL deve acompanhar o pacote Windows; instalação e renderização em Windows ainda precisam ser validadas.

#include <stdio.h>
#include <string.h>
#include <unistd.h> // for sleep()

// �������n�J�ШD (��ڥi�s�����m��API�A�o�̥μ����禡�N��)
int login_attempt(const char* username, const char* password) {
    // ���]���T�K�X�O "correct_pw"
    if (strcmp(password, "correct_pw") == 0) {
        printf("[?] ���\�n�J�I�K�X���G%s\n", password);
        return 1; // �n�J���\
    } else {
        printf("[?] �n�J���ѡA�K�X���աG%s\n", password);
        return 0; // �n�J����
    }
}

int main() {
    const char* username = "student1";

    // �����`���K�X�r��
    const char* password_list[] = {
        "123456", "password", "admin", "student", "student1", 
        "1234", "0000", "1111", "test123", "correct_pw"
    };

    int total = sizeof(password_list) / sizeof(password_list[0]);
    int success = 0;

    printf("=== �����ɤO�}�ѵn�J�����{�� ===\n");
    printf("�ؼбb���G%s\n", username);
    printf("---------------------------------\n");

    for (int i = 0; i < total; i++) {
        printf("[������] ���ղ� %d �ӱK�X...\n", i + 1);
        int result = login_attempt(username, password_list[i]);
        sleep(1); // �����H�����j�]�קK�L�֡^
        if (result) {
            success = 1;
            break;
        }
    }

    if (!success) {
        printf("\n?? �Ҧ��K�X�ҹ��ե��ѡA�b���i��w�Q���m�Ҳի���C\n");
    }

    printf("\n=== ������������ ===\n");
    return 0;
}

